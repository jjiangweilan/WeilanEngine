#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include <spdlog/spdlog.h>

namespace FrameGraph
{
class DeferredMainPassNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(DeferredMainPassNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.shadowMap = AddInputProperty("shadow map", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        output.color = AddOutputProperty("color", PropertyType::Attachment);
        output.depth = AddOutputProperty("depth", PropertyType::Attachment);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        AddConfig<ConfigurableType::ObjectPtr>("cloud noise material", nullptr);

        gbufferPass = Gfx::RG::RenderPass(1, 5);
        Gfx::RG::SubpassAttachment lighting{0};
        Gfx::RG::SubpassAttachment albedo{1};
        Gfx::RG::SubpassAttachment normal{2};
        Gfx::RG::SubpassAttachment property{3};
        Gfx::RG::SubpassAttachment depth{4};
        Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
        gbufferPass.SetSubpass(0, subpassAttachments, depth);

        gbufferPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/GBufferPass.shad");
    }

    void Compile() override {}

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        AttachmentProperty colorProp = input.color->GetValue<AttachmentProperty>();
        AttachmentProperty depthProp = input.depth->GetValue<AttachmentProperty>();
        drawList = input.drawList->GetValue<DrawList*>();

        albedoDesc.SetWidth(colorProp.desc.GetWidth());
        albedoDesc.SetHeight(colorProp.desc.GetHeight());
        albedoDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_SRGB);
        normalDesc.SetWidth(colorProp.desc.GetWidth());
        normalDesc.SetHeight(colorProp.desc.GetHeight());
        normalDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);
        propertyDesc.SetWidth(colorProp.desc.GetWidth());
        propertyDesc.SetHeight(colorProp.desc.GetHeight());
        propertyDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);

        cmd.AllocateAttachment(albedoRTID, albedoDesc);
        cmd.AllocateAttachment(normalRTID, normalDesc);
        cmd.AllocateAttachment(propertyRTID, propertyDesc);

        Gfx::ClearValue clears[] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {1, 0}};

        gbufferPass.SetAttachment(0, colorProp.id);
        gbufferPass.SetAttachment(1, albedoRTID);
        gbufferPass.SetAttachment(2, normalRTID);
        gbufferPass.SetAttachment(3, propertyRTID);
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

        output.color->SetValue(input.color->GetValue<AttachmentProperty>());
        output.depth->SetValue(input.depth->GetValue<AttachmentProperty>());
    }

private:
    const DrawList* drawList;

    glm::vec4* clearValuesVal;

    Gfx::RG::ImageIdentifier albedoRTID = Gfx::RG::ImageIdentifier("gbuffer albedo");
    Gfx::RG::ImageIdentifier normalRTID = Gfx::RG::ImageIdentifier("gbuffer normal");
    Gfx::RG::ImageIdentifier propertyRTID = Gfx::RG::ImageIdentifier("gbuffer property");

    Gfx::RG::ImageDescription albedoDesc;
    Gfx::RG::ImageDescription normalDesc;
    Gfx::RG::ImageDescription propertyDesc;

    Gfx::RG::RenderPass gbufferPass;

    Shader* gbufferPassShader;

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

DEFINE_FRAME_GRAPH_NODE(DeferredMainPassNode, "3D8FE097-C835-41EE-9C9C-8E8667DC4DFD");
} // namespace FrameGraph
  //
