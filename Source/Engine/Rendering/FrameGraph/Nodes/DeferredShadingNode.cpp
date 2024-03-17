#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include <spdlog/spdlog.h>

namespace FrameGraph
{
class DeferredShadingNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(DeferredShadingNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.shadowMap = AddInputProperty("shadow map", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        output.color = AddOutputProperty("color", PropertyType::Attachment);
        output.normal = AddOutputProperty("normal", PropertyType::Attachment);
        output.mask = AddOutputProperty("mask", PropertyType::Attachment);
        output.depth = AddOutputProperty("depth", PropertyType::Attachment);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("cloud noise material", nullptr);

        gbufferPass = Gfx::RG::RenderPass(1, 5);
        Gfx::RG::SubpassAttachment lighting{
            0,
            Gfx::AttachmentLoadOperation::Clear,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::RG::SubpassAttachment albedo{1};
        Gfx::RG::SubpassAttachment normal{2};
        Gfx::RG::SubpassAttachment property{3};
        Gfx::RG::SubpassAttachment depth{4};
        Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
        gbufferPass.SetSubpass(0, subpassAttachments, depth);

        lightingPass = Gfx::RG::RenderPass(1, 1);

        Gfx::RG::SubpassAttachment lightingPassAttachment{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::RG::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
        lightingPass.SetSubpass(0, lightingPassAttachments);

        gbufferPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/GBufferPass.shad");
        lightingPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/StandardPBR.shad");
        lightingPassShaderProgram = lightingPassShader->GetShaderProgram({"G_DEFERRED"});
    }

    void Compile() override
    {
        clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
#if ENGINE_DEV_BUILD
        lightingPassShaderProgram = lightingPassShader->GetShaderProgram({"G_DEFERRED"});
#endif
        AttachmentProperty colorProp = input.color->GetValue<AttachmentProperty>();
        AttachmentProperty depthProp = input.depth->GetValue<AttachmentProperty>();
        drawList = input.drawList->GetValue<DrawList*>();

        int rtWidth = colorProp.desc.GetWidth();
        int rtHeight = colorProp.desc.GetHeight();
        albedoDesc.SetWidth(rtWidth);
        albedoDesc.SetHeight(rtHeight);
        albedoDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_SRGB);
        normalDesc.SetWidth(colorProp.desc.GetWidth());
        normalDesc.SetHeight(colorProp.desc.GetHeight());
        normalDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);
        maskDesc.SetWidth(colorProp.desc.GetWidth());
        maskDesc.SetHeight(colorProp.desc.GetHeight());
        maskDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);

        cmd.AllocateAttachment(albedoRTID, albedoDesc);
        cmd.AllocateAttachment(normalRTID, normalDesc);
        cmd.AllocateAttachment(maskRTID, maskDesc);

        // set scissor and viewport
        Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
        cmd.SetScissor(0, 1, &scissor);
        Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
        cmd.SetViewport(viewport);

        Gfx::ClearValue clears[] = {*clearValuesVal, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};
        gbufferPass.SetAttachment(0, colorProp.id);
        gbufferPass.SetAttachment(1, albedoRTID);
        gbufferPass.SetAttachment(2, normalRTID);
        gbufferPass.SetAttachment(3, maskRTID);
        gbufferPass.SetAttachment(4, depthProp.id);
        cmd.BeginRenderPass(gbufferPass, clears);

        // draw scene objects
        cmd.BindShaderProgram(
            gbufferPassShader->GetDefaultShaderProgram(),
            gbufferPassShader->GetDefaultShaderConfig()
        );

        if (drawList)
        {
            for (auto& draw : *drawList)
            {
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(2, draw.shaderResource);
                cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
        }
        cmd.EndRenderPass();

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}};
        lightingPass.SetAttachment(0, colorProp.id);
        cmd.BeginRenderPass(lightingPass, lightingPassClearValues);
        cmd.SetTexture("albedoTex", albedoRTID);
        cmd.SetTexture("normalTex", normalRTID);
        cmd.SetTexture("maskTex", maskRTID);
        cmd.SetTexture("depthTex", depthProp.id);
        cmd.BindShaderProgram(lightingPassShaderProgram, lightingPassShaderProgram->GetDefaultShaderConfig());
        cmd.Draw(6, 1, 0, 0);
        cmd.EndRenderPass();

        output.color->SetValue(input.color->GetValue<AttachmentProperty>());
        output.depth->SetValue(input.depth->GetValue<AttachmentProperty>());
        output.normal->SetValue(AttachmentProperty{normalRTID, normalDesc});
        output.mask->SetValue(AttachmentProperty{maskRTID, maskDesc});
    }

private:
    const DrawList* drawList;

    glm::vec4* clearValuesVal;

    Gfx::RG::ImageIdentifier albedoRTID = Gfx::RG::ImageIdentifier("gbuffer albedo");
    Gfx::RG::ImageIdentifier normalRTID = Gfx::RG::ImageIdentifier("gbuffer normal");
    Gfx::RG::ImageIdentifier maskRTID = Gfx::RG::ImageIdentifier("gbuffer property");

    Gfx::RG::ImageDescription albedoDesc;
    Gfx::RG::ImageDescription normalDesc;
    Gfx::RG::ImageDescription maskDesc;

    Gfx::RG::RenderPass gbufferPass;
    Gfx::RG::RenderPass lightingPass;

    Shader* gbufferPassShader;
    Shader* lightingPassShader;
    Gfx::ShaderProgram* lightingPassShaderProgram;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
        PropertyHandle shadowMap;
        PropertyHandle drawList;
    } input;

    struct
    {
        PropertyHandle color;
        PropertyHandle normal;
        PropertyHandle mask;
        PropertyHandle depth;
    } output;

    uint32_t cloudTexSize = 128;
    uint32_t cloudTex2Size = 64;

    void MakeCloudNoise(Gfx::CommandBuffer& cmd)
    {
        // Material* cloudNoiseMaterial = GetConfigurableVal<Material*>("cloud noise material");
        //
        // if (cloudNoiseMaterial)
        // {
        //     cloudNoiseMaterial->UploadDataToGPU();
        //     cmd.SetTexture("imgOutput", *cloudRT);
        //     cmd.SetTexture("imgOutput2", *cloud2RT);
        //     cmd.BindShaderProgram(fluidCompute->GetDefaultShaderProgram(), fluidCompute->GetDefaultShaderConfig());
        //     cmd.BindResource(2, cloudNoiseMaterial->GetShaderResource());
        //     cmd.Dispatch(cloudTexSize / 4, cloudTexSize / 4, cloudTexSize / 4);
        //     cmd.SetTexture("cloudDensity", *cloudRT);
        // }
    }
};

DEFINE_FRAME_GRAPH_NODE(DeferredShadingNode, "3D8FE097-C835-41EE-9C9C-8E8667DC4DFD");
} // namespace FrameGraph
  //
