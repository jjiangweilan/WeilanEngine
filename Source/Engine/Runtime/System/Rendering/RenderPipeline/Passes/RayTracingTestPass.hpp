#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
class RayTracingTestPass : public RenderPipelinePass
{
public:
    RayTracingTestPass();
    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::ImageIdentifier& depthTex,
        Gfx::RayTracingSceneHandle tlas,
        Gfx::RayTracingContext* rtContext,
        Gfx::ShaderResource* perSceneResource,
        glm::float2 screenSize
    );

    Gfx::ImageIdentifier GetOutputId() { return outputId; }

    bool DebugBlit(Gfx::ImageIdentifier& dst) override
    {
        dst = outputId;
        return false;
    }

private:
    ObjPtr<Shader> shader;
    Gfx::ImageIdentifier outputId = "RayTracingShadowMask";
};
} // namespace Rendering::Passes
