#pragma once
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBuffer.hpp"
#include "Engine/Runtime/System/Rendering/PipelineGPUBufferAllocator.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelineSetting.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include <memory>

namespace Rendering::Passes
{
class RTGI : public RenderPipelinePass
{
public:
    RTGI();
    ~RTGI() = default;

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

    Gfx::ImageIdentifier& GetOutputId() { return rtgi; }

    bool DebugBlit(Gfx::ImageIdentifier& dst) override;

private:
    void EnsureHistoryBuffers(int width, int height);

    // Ray tracing pass
    Shader* rtgiShader;
    Material mat;

    // SVGF temporal accumulation pass
    Shader* temporalShader;
    Material temporalMat;

    // SVGF à-trous spatial filter pass
    Shader* atrousShader;

    struct AtrousPassResource
    {
        PipelineGPUBuffer paramBuffer = PipelineGPUBufferAllocator::RequestGPUBuffer("RTGI_ATrousParam", PipelineGPUBufferUsage::Uniform);
    };
    AtrousPassResource atrousPassResources[5]; // one per iteration, max 5

    // Transient per-frame render targets
    Gfx::ImageIdentifier rtgiRaw          = "RTGI_Raw";
    Gfx::ImageIdentifier rtgiAccumulated  = "RTGI_Accumulated";
    Gfx::ImageIdentifier momentsOut       = "RTGI_Moments";
    Gfx::ImageIdentifier rtgiPing         = "RTGI_Ping";
    Gfx::ImageIdentifier rtgiPong         = "RTGI_Pong";

    // Final output identifier (points to whichever buffer holds the last à-trous result)
    Gfx::ImageIdentifier rtgi = "RTGI_Output";

    // Persistent cross-frame history buffers
    std::unique_ptr<Gfx::Image> historyColor;
    std::unique_ptr<Gfx::Image> historyMoments; // RGBA: m1, m2, accumCount, unused
    std::unique_ptr<Gfx::Image> historyDepth;   // previous-frame depth (R32G32_SFloat, matches HZB)
    glm::int2 historySize = {0, 0};
    bool historyValid = false;

    bool debugRTGI = false;
};
} // namespace Rendering::Passes
