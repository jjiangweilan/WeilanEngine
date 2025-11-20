#pragma once
#include "DepthAwareBilateralUpsampler.hpp"
#include "Pass.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"

namespace Rendering::Passes
{
class SSAO : public RenderPipelinePass
{
public:
    SSAO();

    Gfx::RenderPass pass = Gfx::RenderPass::SingleColor("SSAO");
    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& halfResDepth,
        const Gfx::ImageIdentifier& fullResDepth,
        const Gfx::RenderImageDescriptor& fullResDepthDesc,
        RenderPipelineSetting* setting
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

    bool needDebug = false;

    /**
     * @brief lazily initialized debug image
     */
    Gfx::ImageIdentifier debugImage;
};
} // namespace Rendering::Passes
