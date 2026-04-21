#pragma once
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBuffer.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include <memory>

namespace Rendering::Passes
{
/// SH-based screen-space GI pass: one sparse ray is traced for each 4x4
/// full-resolution footprint, then resampled into a half-resolution reservoir
/// atlas aligned with the probe tiles. The selected reservoir sample is
/// converted into SH, filtered on the probe atlas, and finally resolved to
/// full resolution.
class GI : public RenderPipelinePass
{
public:
    GI();
    ~GI() = default;

    static uint32_t ComputeAdaptiveRayCount(float accumRatio, uint32_t lowRayCount, uint32_t maxRayCount, float stableAccumFrames);

    static void GetProbeAtlasSize(int width, int height, int& outWidth, int& outHeight)
    {
        outWidth = (width + 1) / 2;
        outHeight = (height + 1) / 2;
    }

    static void GetRayAtlasSize(int width, int height, int& outWidth, int& outHeight)
    {
        outWidth = (width + 3) / 4;
        outHeight = (height + 3) / 4;
    }

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

    // Sparse ray generation pass.
    Shader* giRayGenShader = nullptr;
    Material rayGenMat;

    // Half-res disocclusion classification pass.
    Shader* giDisocclusionShader = nullptr;
    Material disocclusionMat;

    // SH probe accumulation pass.
    Shader* giShader = nullptr;
    Material mat;

    // History-fix pass: build SH + view-Z mip chains for blur.
    Shader* historyFixShader = nullptr;
    Material historyFixMat;

    // SH resolve pass.
    Shader* resolveShader = nullptr;
    Material resolveMat;

    // Probe-atlas recurrent blur pass.
    Shader* blurShader = nullptr;
    Material blurMat;

    struct PostBlurPassResource
    {
        PipelineGPUBuffer inputBuffer =
            PipelineGPUBufferAllocator::RequestGPUBuffer("GI_PostBlur", PipelineGPUBufferUsage::Uniform);
    };

    // Full-resolution spatial post filter pass.
    Shader* postBlurShader = nullptr;

    // Sparse ray atlas outputs (quarter resolution per dimension).
    Gfx::ImageIdentifier giRayData = "GI_RayData";
    Gfx::ImageIdentifier giRayMeta = "GI_RayMeta";

    // Half-res probe disocclusion mask.
    Gfx::ImageIdentifier giDisocclusionMask = "GI_DisocclusionMask";

    // SH probe output identifiers (half resolution per dimension).
    Gfx::ImageIdentifier giSH0 = "GI_SH0";
    Gfx::ImageIdentifier giSH1 = "GI_SH1";
    Gfx::ImageIdentifier giSH2 = "GI_SH2";
    Gfx::ImageIdentifier giAccumulationCount = "GI_AccumulationCount";

    // History-fix mip-chain outputs.
    Gfx::ImageIdentifier giHistoryFixSH0 = "GI_HistoryFixSH0";
    Gfx::ImageIdentifier giHistoryFixSH1 = "GI_HistoryFixSH1";
    Gfx::ImageIdentifier giHistoryFixSH2 = "GI_HistoryFixSH2";
    Gfx::ImageIdentifier giHistoryFixViewZ = "GI_HistoryFixViewZ";

    // Probe blur transient identifiers.
    Gfx::ImageIdentifier giBlurredSH0 = "GI_BlurredSH0";
    Gfx::ImageIdentifier giBlurredSH1 = "GI_BlurredSH1";
    Gfx::ImageIdentifier giBlurredSH2 = "GI_BlurredSH2";

    // Full-resolution a-trous ping-pong intermediates.
    Gfx::ImageIdentifier giPostBlurAtrousA = "GI_PostBlurAtrousA";
    Gfx::ImageIdentifier giPostBlurAtrousB = "GI_PostBlurAtrousB";

    std::vector<PostBlurPassResource> postBlurPassResources;

    // Final full-resolution resolve output.
    Gfx::ImageIdentifier giIrradiance  = "GI_Irradiance";
    Gfx::ImageIdentifier giOutput      = "GI_Output";

    // Persistent cross-frame SH history buffers (probe atlas, half resolution).
    std::unique_ptr<Gfx::Image> historySH0;
    std::unique_ptr<Gfx::Image> historySH1;
    std::unique_ptr<Gfx::Image> historySH2;
    std::unique_ptr<Gfx::Image> historyAccumulationCount;

    // Persistent full-resolution reprojection history for GI.slang.
    std::unique_ptr<Gfx::Image> historyDepth;
    std::unique_ptr<Gfx::Image> historyNormal;
    std::unique_ptr<Gfx::Image> historyIrradiance;

    glm::int2 historySize = {0, 0};

    std::unique_ptr<Gfx::Buffer> haltonBuffer;

    bool debugGI = false;
    bool debugGIAccumulation = false;
};
} // namespace Rendering::Passes
