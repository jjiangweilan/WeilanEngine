#pragma once
#include "DepthAwareBilateralUpsampler.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Pass.hpp"

namespace Rendering::Passes
{
class SSAO : public RenderPipelinePass
{
public:
    SSAO();

    Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("SSAO");
    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& hizTex,
        const Gfx::ImageIdentifier& halfResDepth,
        const Gfx::ImageIdentifier& fullResDepth,
        const Gfx::RenderImageDescriptor& fullResDepthDesc,
        RenderPipelineSetting* setting,
        RenderingData& renderingData
    );

    Gfx::ImageIdentifier& GetSSAOTex() { return result == nullptr ? ssao : *result; }
    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    ObjPtr<Shader> ssaoShader;
    DepthAwareBilateralUpsampler upscaler;
    Material mat;
    Material bilateralMat;
    Gfx::ImageIdentifier ssaoDownSampled = Gfx::ImageIdentifier("SSAO Down Sampled");
    Gfx::ImageIdentifier ssao = Gfx::ImageIdentifier("SSAO");
    Gfx::ImageIdentifier* result = nullptr;

    bool debugFinalSSAO = false;
    bool debugNormal = false;

    /**
     * @brief lazily initialized debug image
     */
    Gfx::ImageIdentifier debugImage;
};
} // namespace Rendering::Passes
