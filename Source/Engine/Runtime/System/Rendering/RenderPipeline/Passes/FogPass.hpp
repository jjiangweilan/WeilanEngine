#pragma once
#include "Engine/Driver/GfxDriver/GfxStruct.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/GPUBuffer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include "FogPassParameters.hpp"

#include "Engine/Shaders/DepthBasedFogInput.hlsl"

namespace Rendering::Passes
{
class FogPass : public RenderPipelinePass // Now derives from RenderPipelinePass
{
    ObjPtr<Shader> shader;
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
