#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include <spdlog/spdlog.h>

namespace FrameGraph
{
class LightingPassNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(LightingPassNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment);
        input.albedo = AddInputProperty("albedo", PropertyType::Attachment);
        input.normal = AddInputProperty("normal", PropertyType::Attachment);
        input.mask = AddInputProperty("mask", PropertyType::Attachment);
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

        lightingPass = Gfx::RG::RenderPass(1, 1);

        Gfx::RG::SubpassAttachment lightingPassAttachment{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store};
        Gfx::RG::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
        lightingPass.SetSubpass(0, lightingPassAttachments);

        lightingPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/StandardPBR.shad");
        lightingPassShaderProgram = lightingPassShader->GetShaderProgram({"G_DEFERRED"});
        lightingPassConfig = lightingPassShaderProgram->GetDefaultShaderConfig();
        lightingPassConfig.color.blends.push_back({
            .blendEnable = true,
            .srcColorBlendFactor = Gfx::BlendFactor::One,
            .dstColorBlendFactor = Gfx::BlendFactor::One,
            .colorBlendOp = Gfx::BlendOp::Add,
            .srcAlphaBlendFactor = Gfx::BlendFactor::One,
            .dstAlphaBlendFactor = Gfx::BlendFactor::Zero,
            .alphaBlendOp = Gfx::BlendOp::Add,
            .colorWriteMask = Gfx::ColorComponentBit::Component_All_Bits,
        });
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
        AttachmentProperty albedoProp = input.albedo->GetValue<AttachmentProperty>();
        AttachmentProperty normalProp = input.normal->GetValue<AttachmentProperty>();
        AttachmentProperty maskProp = input.mask->GetValue<AttachmentProperty>();
        AttachmentProperty depthProp = input.depth->GetValue<AttachmentProperty>();
        drawList = input.drawList->GetValue<DrawList*>();

        int rtWidth = depthProp.desc.GetWidth();
        int rtHeight = depthProp.desc.GetHeight();

        // set scissor and viewport
        Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
        cmd.SetScissor(0, 1, &scissor);
        Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
        cmd.SetViewport(viewport);

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}};
        lightingPass.SetAttachment(0, colorProp.id);
        cmd.BeginRenderPass(lightingPass, lightingPassClearValues);
        cmd.SetTexture("albedoTex", albedoProp.id);
        cmd.SetTexture("normalTex", normalProp.id);
        cmd.SetTexture("maskTex", maskProp.id);
        cmd.SetTexture("depthTex", depthProp.id);
        cmd.BindShaderProgram(lightingPassShaderProgram, lightingPassConfig);
        cmd.Draw(6, 1, 0, 0);
        cmd.EndRenderPass();

        output.color->SetValue(colorProp);
        output.normal->SetValue(normalProp);
        output.mask->SetValue(maskProp);
        output.depth->SetValue(depthProp);
    }

private:
    const DrawList* drawList;

    glm::vec4* clearValuesVal;

    Gfx::RG::ImageDescription albedoDesc;
    Gfx::RG::ImageDescription normalDesc;
    Gfx::RG::ImageDescription maskDesc;

    Gfx::RG::RenderPass lightingPass;

    Shader* lightingPassShader;
    Gfx::ShaderProgram* lightingPassShaderProgram;
    Gfx::ShaderConfig lightingPassConfig;

    struct
    {
        PropertyHandle color;
        PropertyHandle albedo;
        PropertyHandle normal;
        PropertyHandle mask;
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
};

DEFINE_FRAME_GRAPH_NODE(LightingPassNode, "92AECADA-9A17-4D88-9A01-FBB8F19E3173");
} // namespace FrameGraph
  //
