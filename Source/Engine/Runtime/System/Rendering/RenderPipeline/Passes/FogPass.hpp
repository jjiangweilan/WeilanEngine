#pragma once
#include "FogPassParameters.hpp"
#include "Driver/GfxDriver/Image.hpp"
#include "Driver/GfxDriver/RenderGraph.hpp"
#include "Driver/GfxDriver/ShaderResource.hpp"
#include "Runtime/System/Rendering/GPUBuffer.hpp"
#include "Runtime/System/Rendering/ShaderLibrary.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"

#include "Shaders/DepthBasedFogInput.hlsl"

namespace Rendering::Passes
{
class FogPass : public RenderPipelinePass // Now derives from RenderPipelinePass
{
    ObjPtr<Shader> shader;
    std::unique_ptr<Gfx::ShaderResource> shaderInput;
    GPUBuffer<DepthBasedFogParams> fogInputBuffer;

public:
    FogPass();
    void Execute(
        Gfx::CommandBuffer& cmd,
        Gfx::ImageIdentifier& outputColor,
        Gfx::ImageIdentifier& depthCopy,
        const FogPassParameters& parameters
    );
};
} // namespace Rendering::Passes
