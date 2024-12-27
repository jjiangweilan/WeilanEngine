#include "../NodeBlueprint.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include "Rendering/Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

namespace Rendering::FrameGraph
{
class LightingPassNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(LightingPassNode)
    {
        input.color = AddInputProperty("color", PropertyType::Attachment, 1);
        input.albedo = AddInputProperty("albedo", PropertyType::Attachment, 2);
        input.normal = AddInputProperty("normal", PropertyType::Attachment, 3);
        input.mask = AddInputProperty("mask", PropertyType::Attachment, 4);
        input.ao = AddInputProperty("ao", PropertyType::Attachment, 5);
        input.depth = AddInputProperty("depth", PropertyType::Attachment, 6);
        input.shadowMap = AddInputProperty("shadow map", PropertyType::Attachment, 7);
        input.drawList = AddInputProperty("draw list", PropertyType::DrawListPointer, 8);

        output.color = AddOutputProperty("color", PropertyType::Attachment, 9);
        output.normal = AddOutputProperty("normal", PropertyType::Attachment, 10);
        output.mask = AddOutputProperty("mask", PropertyType::Attachment, 11);
        output.depth = AddOutputProperty("depth", PropertyType::Attachment, 12);

        input.tileBuffer = AddInputProperty("tile buffer", PropertyType::GfxBuffer, 13);

        AddConfig<ConfigurableType::Vec4>("clear values", glm::vec4{52 / 255.0f, 177 / 255.0f, 235 / 255.0f, 1});
        AddConfig<ConfigurableType::ObjectPtr>("skybox", nullptr);
        AddConfig<ConfigurableType::Float>("shadow constant bias", 0.0003f);
        AddConfig<ConfigurableType::Float>("shadow normal bias", 0.1f);

        lightingPass = Gfx::RG::RenderPass(1, 1);

        Gfx::RG::SubpassAttachment lightingPassAttachment{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store
        };
        Gfx::RG::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
        lightingPass.SetSubpass(0, lightingPassAttachments);

        lightingPassShader = ShaderLibrary::GetShader(ShaderLibrary::DeferredPBRShading);
        auto lightingPassConfig_t = *lightingPassShader->GetShaderProgram()->GetDefaultShaderConfig();
        lightingPassConfig_t.color.blends.push_back({
            .blendEnable = true,
            .srcColorBlendFactor = Gfx::BlendFactor::One,
            .dstColorBlendFactor = Gfx::BlendFactor::One,
            .colorBlendOp = Gfx::BlendOp::Add,
            .srcAlphaBlendFactor = Gfx::BlendFactor::One,
            .dstAlphaBlendFactor = Gfx::BlendFactor::Zero,
            .alphaBlendOp = Gfx::BlendOp::Add,
            .colorWriteMask = Gfx::ColorComponentBit::Component_All_Bits,
        });
        lightingPassConfig = lightingPassConfig_t;

        shaderResource = GetGfxDriver()->CreateShaderResource();
        shadingPropertiesBuffer = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            .usages = Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            .size = sizeof(ShadingProperties),
            .visibleInCPU = false,
            .debugName = "lighting pass buffer",
            .gpuWrite = false
        });
        shaderResource->SetBuffer("ShadingProperties", shadingPropertiesBuffer.get());
    }

    void Compile() override
    {
        clearValuesVal = GetConfigurablePtr<glm::vec4>("clear values");
        config.shadowConstantBias = GetConfigurablePtr<float>("shadow constant bias");
        config.shadowNormalBias = GetConfigurablePtr<float>("shadow normal bias");
    }

    void Execute(RenderingContext& renderContext, RenderingData& renderingData) override
    {
        auto& cmd = *renderingData.cmd;
        // upload buffer
        prop.shadowConstantBias = *config.shadowConstantBias;
        prop.shadowNormalBias = *config.shadowNormalBias;
        GetGfxDriver()->UploadBuffer(*shadingPropertiesBuffer, (uint8_t*)&prop, shadingPropertiesBuffer->GetSize());

        AttachmentProperty colorProp = input.color->GetValue<AttachmentProperty>();
        AttachmentProperty albedoProp = input.albedo->GetValue<AttachmentProperty>();
        AttachmentProperty normalProp = input.normal->GetValue<AttachmentProperty>();
        AttachmentProperty aoProp = input.ao->GetValue<AttachmentProperty>();
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
        cmd.SetTexture("aoMap", aoProp.id);
        cmd.BindShaderProgram(lightingPassShader->GetShaderProgram(), lightingPassConfig);
        cmd.BindResource(1, shaderResource.get());
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

    ObjPtr<Shader2> lightingPassShader;
    Gfx::PipelineConfig lightingPassConfig;

    struct
    {
        PropertyHandle color;
        PropertyHandle albedo;
        PropertyHandle normal;
        PropertyHandle mask;
        PropertyHandle ao;
        PropertyHandle depth;
        PropertyHandle shadowMap;
        PropertyHandle drawList;
        PropertyHandle tileBuffer;
    } input;

    struct
    {
        PropertyHandle color;
        PropertyHandle normal;
        PropertyHandle mask;
        PropertyHandle depth;
    } output;

    struct ShadingProperties
    {
        float shadowConstantBias;
        float shadowNormalBias;
    } prop;

    struct Configs
    {
        float* shadowConstantBias;
        float* shadowNormalBias;
    } config;
    std::unique_ptr<Gfx::Buffer> shadingPropertiesBuffer;
    std::unique_ptr<Gfx::ShaderResource> shaderResource;
};

DEFINE_FRAME_GRAPH_NODE(LightingPassNode, "92AECADA-9A17-4D88-9A01-FBB8F19E3173");
} // namespace Rendering::FrameGraph
  //
