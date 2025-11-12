#pragma once
#include "FogPassParameters.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/RenderGraph.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/GPUBuffer.hpp"
#include "Rendering/ShaderLibrary.hpp"

#include "Shaders/DepthBasedFogInput.hlsl"

namespace Rendering::Passes
{
    class FogPass
    {
        ObjPtr<Shader> shader;
        std::unique_ptr<Gfx::ShaderResource> shaderInput;
        GPUBuffer<DepthBasedFogParams> fogInputBuffer;

    public:
        FogPass();
        void Execute(Gfx::CommandBuffer& cmd, Gfx::ImageIdentifier& outputColor, Gfx::ImageIdentifier& depthCopy, const FogPassParameters& parameters);
    };
}
