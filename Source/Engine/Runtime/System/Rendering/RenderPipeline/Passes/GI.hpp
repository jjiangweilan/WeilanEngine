#pragma once
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <memory>

namespace Rendering::Passes
{
/// SH-based screen-space GI pass: 1 sample per 4x4 tile (inline RT),
/// radiance encoded into Linear SH (3 RGBA16F textures), temporally blended via EMA.
/// Full-res irradiance is resolved from SH and optionally denoised with SVGF.
class GI : public RenderPipelinePass
{
public:
    GI();
    ~GI() = default;

    void Execute(
        Gfx::CommandBuffer* cmd,
        const Gfx::ImageIdentifier& hizTex,
        const Gfx::ImageIdentifier& albedoTex,
        const Gfx::ImageIdentifier& normalTex,
        const Gfx::ImageIdentifier& motionVectorTex,
        RenderPipelineSetting* setting,
        RenderingData& renderingData,
        Gfx::RayTracingSceneHandle tlas,
        Gfx::RayTracingContext* rtContext
    );

    Gfx::ImageIdentifier& GetOutputId() { return giOutput; }
    Gfx::ImageIdentifier& GetOutputSH0() { return giSH0; }
    Gfx::ImageIdentifier& GetOutputSH1() { return giSH1; }
    Gfx::ImageIdentifier& GetOutputSH2() { return giSH2; }
    Gfx::ImageIdentifier& GetGIOutput() { return giOutput; }

    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    void EnsureHistoryBuffers(int width, int height);

    // SH compute pass
    Shader* giShader;
    Material mat;

    // SH resolve pass
    Shader* resolveShader;
    Material resolveMat;

    // SVGF temporal pass
    Shader* temporalShader;
    Material temporalMat;

    // SVGF variance prefilter pass
    Shader* varianceShader;
    Material varianceMat;

    // SVGF à-trous pass
    Shader* atrousShader;

    // SH output identifiers (quarter resolution)
    Gfx::ImageIdentifier giSH0 = "GI_SH0";
    Gfx::ImageIdentifier giSH1 = "GI_SH1";
    Gfx::ImageIdentifier giSH2 = "GI_SH2";

    // Resolve + SVGF transient identifiers (full resolution)
    Gfx::ImageIdentifier giIrradiance  = "GI_Irradiance";
    Gfx::ImageIdentifier giTemporalOut = "GI_TemporalOut";
    Gfx::ImageIdentifier giMomentsOut  = "GI_MomentsOut";
    Gfx::ImageIdentifier giVarianceOut = "GI_VarianceOut";
    Gfx::ImageIdentifier giAtrousA     = "GI_ATrousA";
    Gfx::ImageIdentifier giAtrousB     = "GI_ATrousB";
    Gfx::ImageIdentifier giOutput      = "GI_Output";

    // Persistent cross-frame SH history buffers (quarter resolution)
    std::unique_ptr<Gfx::Image> historySH0;
    std::unique_ptr<Gfx::Image> historySH1;
    std::unique_ptr<Gfx::Image> historySH2;

    // Persistent cross-frame SVGF history buffers (full resolution)
    std::unique_ptr<Gfx::Image> historyColor;
    std::unique_ptr<Gfx::Image> historyMoments;
    std::unique_ptr<Gfx::Image> historyDepth;
    std::unique_ptr<Gfx::Image> historyNormal;

    glm::int2 historySize = {0, 0};
    bool historyValid = false;
    bool svgfHistoryValid = false;

    std::unique_ptr<Gfx::Buffer> haltonBuffer;

    bool debugGI = false;

    void GetQuarterSize(int width, int height, int& outWidth, int& outHeight) const
    {
        outWidth = (width + 1) / 2;
        outHeight = (height + 1) / 2;
    }
};
} // namespace Rendering::Passes
