#pragma once
#include "DepthAwareBilateralUpsampler.hpp"
#include "Pass.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Rendering/Shader2.hpp"

namespace Rendering::Passes
{
class SSAO : public RenderingModule
{
public:
    SSAO();

    Gfx::RG::RenderPass pass = Gfx::RG::RenderPass::SingleColor("SSAO");
    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::RG::ImageIdentifier& halfResDepth,
        const Gfx::RG::ImageIdentifier& fullResDepth,
        const Gfx::RG::RenderImageDescriptor& fullResDepthDesc,
        RenderPipelineSetting* setting
    );

    const Gfx::RG::ImageIdentifier& GetSSAOTex() { return result == nullptr ? ssao : *result; }
    bool DebugBlit(Gfx::RG::ImageIdentifier& dst) override;

private:
    ObjPtr<Shader2> ssaoShader;
    DepthAwareBilateralUpsampler upscaler;
    Material mat;
    Material bilateralMat;
    Gfx::RG::ImageIdentifier ssaoDownSampled = Gfx::RG::ImageIdentifier("SSAO Down Sampled");
    Gfx::RG::ImageIdentifier ssao = Gfx::RG::ImageIdentifier("SSAO");
    Gfx::RG::ImageIdentifier* result = nullptr;

    bool needDebug = false;

    /**
     * @brief lazily initialized debug image
     */
    Gfx::RG::ImageIdentifier debugImage;
};
} // namespace Rendering::Passes
