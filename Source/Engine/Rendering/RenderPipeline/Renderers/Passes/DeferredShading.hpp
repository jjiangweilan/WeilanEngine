#pragma once
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "Rendering/DrawList.hpp"
#include "Rendering/RenderingData.hpp"
#include <spdlog/spdlog.h>

namespace Rendering
{
class DeferredShading
{
public:
    struct Setting
    {
        glm::vec4 clearValues;
        float shadowConstantBias = 0.0003f;
        float shadowNormalBias = 0.1f;
    };

    struct
    {
        Gfx::RG::ImageIdentifier color;
        Gfx::RG::ImageDescription colorDesc;
        Gfx::RG::ImageIdentifier albedo;
        Gfx::RG::ImageIdentifier normal;
        Gfx::RG::ImageIdentifier mask;
        Gfx::RG::ImageIdentifier depth;
        Gfx::RG::ImageIdentifier shadowMap;
        Gfx::Buffer* tileBuffer;
    } input;

    DeferredShading()
    {
        lightingPass = Gfx::RG::RenderPass(1, 1);

        Gfx::RG::SubpassAttachment lightingPassAttachment{
            0,
            Gfx::AttachmentLoadOperation::Load,
            Gfx::AttachmentStoreOperation::Store};
        Gfx::RG::SubpassAttachment lightingPassAttachments[] = {lightingPassAttachment};
        lightingPass.SetSubpass(0, lightingPassAttachments);

        lightingPassShader =
            (Shader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Game/StandardPBRLighting.shad");
        lightingPassShaderProgram = lightingPassShader->GetShaderProgram(0, 0);
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

        shaderResource = GetGfxDriver()->CreateShaderResource();
        shadingPropertiesBuffer = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            .usages = Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            .size = sizeof(ShadingProperties),
            .visibleInCPU = false,
            .debugName = "lighting pass buffer",
            .gpuWrite = false});
        shaderResource->SetBuffer("ShadingProperties", shadingPropertiesBuffer.get());
    }

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& frameData, Setting& setting)
    {
        // upload buffer
        prop.shadowConstantBias = *config.shadowConstantBias;
        prop.shadowNormalBias = *config.shadowNormalBias;
        GetGfxDriver()->UploadBuffer(*shadingPropertiesBuffer, (uint8_t*)&prop, shadingPropertiesBuffer->GetSize());

        int rtWidth = input.colorDesc.GetWidth();
        int rtHeight = input.colorDesc.GetHeight();

        // set scissor and viewport
        Rect2D scissor = {{0, 0}, {static_cast<uint32_t>(rtWidth), static_cast<uint32_t>(rtHeight)}};
        cmd.SetScissor(0, 1, &scissor);
        Gfx::Viewport viewport{0, 0, static_cast<float>(rtWidth), static_cast<float>(rtHeight), 0, 1};
        cmd.SetViewport(viewport);

#if ENGINE_DEV_BUILD
        lightingPassShaderProgram = lightingPassShader->GetShaderProgram(0, 0);
#endif

        Gfx::ClearValue lightingPassClearValues[] = {{0, 0, 0, 0}};
        lightingPass.SetAttachment(0, input.color);
        cmd.BeginRenderPass(lightingPass, lightingPassClearValues);
        cmd.SetTexture("albedoTex", input.albedo);
        cmd.SetTexture("normalTex", input.normal);
        cmd.SetTexture("maskTex", input.mask);
        cmd.SetTexture("depthTex", input.depth);
        cmd.BindShaderProgram(lightingPassShaderProgram, lightingPassConfig);
        cmd.BindResource(1, shaderResource.get());
        cmd.Draw(6, 1, 0, 0);
        cmd.EndRenderPass();
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
} // namespace Rendering
