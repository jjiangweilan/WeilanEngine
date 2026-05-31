#pragma once
#include "Engine/Core/Ptr.hpp"
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include <memory>
#include <span>

namespace Rendering::Passes
{
class DynamicMotionVectorPass : public RenderPipelinePass
{
public:
    DynamicMotionVectorPass();
    ~DynamicMotionVectorPass() override = default;

    void Execute(
        Gfx::CommandBuffer& cmd,
        const Gfx::ImageIdentifier& motionVector,
        const Gfx::ImageIdentifier& depth,
        Gfx::ShaderResource* globalResource,
        Gfx::Buffer* indirectCommandBuffer,
        std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
        std::span<const float4x4> previousFrameWorldMatrices
    );

private:
    Gfx::Buffer* UploadDynamicMotionData(Gfx::CommandBuffer& cmd, std::span<const float4x4> previousFrameWorldMatrices);

    ObjPtr<Shader> shader;
    std::unique_ptr<Gfx::ShaderResource> resource;
    std::unique_ptr<Gfx::Buffer> dynamicMotionPreviousModelBuffer;
    uint32_t dynamicMotionPreviousModelCapacity = 0;
};
} // namespace Rendering::Passes
