#include "DynamicMotionVectorPass.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
DynamicMotionVectorPass::DynamicMotionVectorPass()
{
    shader = ShaderLibrary::GetShader(Shaders::PostProcess_DynamicMotionVector);
    resource = GetGfxDriver()->CreateShaderResource();
}

void DynamicMotionVectorPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& motionVector,
    const Gfx::ImageIdentifier& depth,
    Gfx::ShaderResource* globalResource,
    Gfx::Buffer* indirectCommandBuffer,
    std::span<const GPUObjectShaderGroup> gpuObjectShaderGroups,
    std::span<const float4x4> previousFrameWorldMatrices
)
{
    auto* previousModelsBuffer = UploadDynamicMotionData(cmd, previousFrameWorldMatrices);
    if (!previousModelsBuffer || !indirectCommandBuffer || gpuObjectShaderGroups.empty())
        return;

    cmd.BeginLabel("DynamicMotionVector", {0.1f, 0.7f, 0.7f, 1.0f});

    Gfx::RenderAttachment attachments[] = {
        {motionVector, Gfx::AttachmentLoadOperation::Load},
        {depth, Gfx::AttachmentLoadOperation::Load},
    };
    Gfx::ClearValue clears[] = {{0.0f, 0.0f, 0.0f, 0.0f}, {0, 0}};
    cmd.BeginRenderPass(attachments, clears);

    auto* shaderProgram = shader->GetShaderProgram();
    resource->SetBuffer("dynamicMotionPreviousModels", previousModelsBuffer);
    cmd.BindResource(shader->GetSet(Gfx::DescriptorSetSemantics::Global), globalResource);
    cmd.BindResource(shader->GetSet(Gfx::DescriptorSetSemantics::Material), resource.get());
    cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());

    struct PushConstant
    {
        uint32_t firstGpuObjectOffset = 0;
    } pconst;

    for (const auto& group : gpuObjectShaderGroups)
    {
        if (!group.hasMotion)
            continue;

        pconst.firstGpuObjectOffset = group.firstDrawIndex;
        cmd.SetPushConstant(shaderProgram, &pconst);
        cmd.DrawIndexedIndirect(
            indirectCommandBuffer,
            group.firstDrawIndex * sizeof(DrawIndexedIndirectCommand),
            group.drawCount,
            sizeof(DrawIndexedIndirectCommand)
        );
    }

    cmd.EndRenderPass();

    cmd.EndLabel();
}

Gfx::Buffer* DynamicMotionVectorPass::UploadDynamicMotionData(
    Gfx::CommandBuffer& cmd,
    std::span<const float4x4> previousFrameWorldMatrices
)
{
    if (previousFrameWorldMatrices.empty())
        return nullptr;

    if (previousFrameWorldMatrices.size() > dynamicMotionPreviousModelCapacity)
    {
        uint32_t newCapacity = dynamicMotionPreviousModelCapacity == 0 ? 256 : dynamicMotionPreviousModelCapacity;
        while (newCapacity < previousFrameWorldMatrices.size())
            newCapacity *= 2;

        dynamicMotionPreviousModelBuffer = GetGfxDriver()->CreateBuffer(
            newCapacity * sizeof(float4x4),
            Gfx::BufferUsage::Storage | Gfx::BufferUsage::Transfer_Dst,
            false,
            false,
            "DynamicMotionPreviousModels"
        );
        dynamicMotionPreviousModelCapacity = newCapacity;
    }

    cmd.UploadData(
        *dynamicMotionPreviousModelBuffer,
        const_cast<float4x4*>(previousFrameWorldMatrices.data()),
        previousFrameWorldMatrices.size() * sizeof(float4x4),
        0
    );

    return dynamicMotionPreviousModelBuffer.get();
}
} // namespace Rendering::Passes
