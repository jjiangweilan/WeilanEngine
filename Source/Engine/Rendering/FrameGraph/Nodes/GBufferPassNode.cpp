#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include <spdlog/spdlog.h>

namespace FrameGraph
{
class GBufferPassNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(GBufferPassNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.depth = AddInputProperty("depth", PropertyType::Attachment);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer);

        output.color = AddOutputProperty("color", PropertyType::Attachment);
        output.albedo = AddOutputProperty("albedo", PropertyType::Attachment);
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
            Gfx::AttachmentStoreOperation::Store};
        Gfx::RG::SubpassAttachment albedo{1};
        Gfx::RG::SubpassAttachment normal{2};
        Gfx::RG::SubpassAttachment property{3};
        Gfx::RG::SubpassAttachment depth{4};
        Gfx::RG::SubpassAttachment subpassAttachments[] = {lighting, albedo, normal, property};
        gbufferPass.SetSubpass(0, subpassAttachments, depth);

        gbufferPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/GBufferPass.shad");
        alphaTestFeatureID = gbufferPassShader->GetFeaturesID({"_AlphaTest"});
    }

    void Compile() override
    {
        clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        AttachmentProperty colorProp = input.color->GetValue<AttachmentProperty>();
        AttachmentProperty depthProp = input.depth->GetValue<AttachmentProperty>();
        drawList = input.drawList->GetValue<DrawList*>();

        int rtWidth = depthProp.desc.GetWidth();
        int rtHeight = depthProp.desc.GetHeight();
        albedoDesc.SetWidth(rtWidth);
        albedoDesc.SetHeight(rtHeight);
        albedoDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_SRGB);
        normalDesc.SetWidth(depthProp.desc.GetWidth());
        normalDesc.SetHeight(depthProp.desc.GetHeight());
        normalDesc.SetFormat(Gfx::ImageFormat::R8G8B8A8_UNorm);
        maskDesc.SetWidth(depthProp.desc.GetWidth());
        maskDesc.SetHeight(depthProp.desc.GetHeight());
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

        if (drawList)
        {
            cmd.BindShaderProgram(
                gbufferPassShader->GetDefaultShaderProgram(),
                gbufferPassShader->GetDefaultShaderConfig()
            );
            for (int i = 0; i < drawList->alphaTestIndex; ++i)
            {
                auto& draw = drawList->at(i);
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(2, draw.shaderResource);
                cmd.BindShaderProgram(gbufferPassShader->GetDefaultShaderProgram(), *draw.shaderConfig);
                cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }

            auto alphaTestedShader = gbufferPassShader->GetShaderProgram(alphaTestFeatureID);
            for (int i = drawList->alphaTestIndex; i < drawList->transparentIndex; ++i)
            {
                auto& draw = drawList->at(i);
                cmd.BindVertexBuffer(draw.vertexBufferBinding, 0);
                cmd.BindIndexBuffer(draw.indexBuffer, 0, draw.indexBufferType);
                cmd.BindResource(2, draw.shaderResource);
                cmd.BindShaderProgram(alphaTestedShader, *draw.shaderConfig);
                cmd.SetPushConstant(draw.shader, (void*)&draw.pushConstant);
                cmd.DrawIndexed(draw.indexCount, 1, 0, 0, 0);
            }
        }
        cmd.EndRenderPass();

        output.color->SetValue(colorProp);
        output.albedo->SetValue(AttachmentProperty{albedoRTID, albedoDesc});
        output.depth->SetValue(depthProp);
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
    uint64_t alphaTestFeatureID;

    Shader* gbufferPassShader;

    struct
    {
        PropertyHandle color;
        PropertyHandle depth;
        PropertyHandle drawList;
    } input;

    struct
    {
        PropertyHandle color;
        PropertyHandle albedo;
        PropertyHandle normal;
        PropertyHandle mask;
        PropertyHandle depth;
    } output;
};

DEFINE_FRAME_GRAPH_NODE(GBufferPassNode, "BA901CE6-84C1-4689-A0E3-87E548314A45");
} // namespace FrameGraph
  //
