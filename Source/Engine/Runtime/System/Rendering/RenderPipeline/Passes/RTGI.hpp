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
    // Ray tracing pass
    Shader* rtgiShader;
    Material mat;

    // Transient per-frame render targets
    Gfx::ImageIdentifier rtgiRaw          = "RTGI_Raw";

    // Final output identifier
    Gfx::ImageIdentifier rtgi = "RTGI_Output";

    bool debugRTGI = false;
};
} // namespace Rendering::Passes
