#pragma once
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"

namespace Rendering::Passes
{
class SSIL : public RenderPipelinePass
{
public:
    class BilateralFilterPass
    {
    public:
        BilateralFilterPass();

        float depthDiffSigma = 1.0f;

        void Execute(
            Gfx::CommandBuffer* cmd,
            const Gfx::ImageIdentifier& sourceTex,
            glm::int2 sourceTexSize,
            const Gfx::ImageIdentifier& lowDepth,
            const Gfx::ImageIdentifier& highDepth,
            const Gfx::ImageIdentifier& destination,
            int lowDepthMipLevel
        );

    private:
        Material mat;
        Shader* shader;
    };

    SSIL();
    ~SSIL() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& colorTex,
        const Gfx::ImageIdentifier& hizTex,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& normalTex,
        const Gfx::ImageIdentifier& targetColor,
        RenderPipelineSetting* setting,
        RenderingData& renderingData
    );

    Gfx::ImageIdentifier& GetOutputId() { return ssil; }

    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    Shader* ssilShader;
    Shader* bilateralUpscale;
    Material mat;
    Gfx::ImageIdentifier ssilRaw = "SSIL_Raw";
    Gfx::ImageIdentifier ssil = "SSIL_Output";
    Gfx::ImageIdentifier firstFilterPassOutput = "SSIL_Filter1";
    std::unique_ptr<BilateralFilterPass> firstFilterPass;
    std::unique_ptr<BilateralFilterPass> secondFilterPass;

    Gfx::PipelineConfig combineConfig;

    bool debugSSIL = false;
};
} // namespace Rendering::Passes
