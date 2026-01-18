#include "RenderCommandBuffer.hpp"
#include "CommandStreamProcessor.hpp"
#include "RenderResourceAllocator.hpp"

void RenderCommandBuffer::AllocateTempImage(const Gfx::RenderImageDescriptor& desc, Gfx::ImageIdentifier& id)
{
    resourceAllocator->Request(id, desc);
}

void RenderCommandBuffer::BeginLabel(std::string_view label, const float4& color)
{
    char* extraData = nullptr;
    BeginLabelCmd* ptr = cm.Push<BeginLabelCmd>(
        &CommandStreamProcessor::BeginLabel,
        &extraData,
        label.length() + 1
    );

    memcpy(extraData, label.data(), label.length() + 1);
    ptr->str = extraData;
    ptr->color = color;
}

void RenderCommandBuffer::EndLabel()
{
    cm.Push<EndLabelCmd>(&CommandStreamProcessor::EndLabel);
}

void RenderCommandBuffer::SetRenderPass(std::span<const Gfx::RenderAttachment> images)
{
    Gfx::RenderAttachment* extraData = nullptr;
    SetRenderPassCmd* ptr = cm.Push<SetRenderPassCmd>(
        &CommandStreamProcessor::SetRenderPass,
        &extraData,
        sizeof(Gfx::RenderAttachment) * images.size()
    );

    if (!images.empty())
        memcpy((void*)extraData, images.data(), sizeof(Gfx::RenderAttachment) * images.size());

    ptr->attachments = extraData;
    ptr->count = static_cast<uint32_t>(images.size());
}

void RenderCommandBuffer::SetClearValues(std::span<Gfx::ClearValue> clearValues)
{
    Gfx::ClearValue* extraData = nullptr;
    SetClearValuesCmd* ptr = cm.Push<SetClearValuesCmd>(
        &CommandStreamProcessor::SetClearValues,
        &extraData,
        sizeof(Gfx::ClearValue) * clearValues.size()
    );

    if (!clearValues.empty())
        memcpy(extraData, clearValues.data(), sizeof(Gfx::ClearValue) * clearValues.size());

    ptr->values = extraData;
    ptr->count = static_cast<uint32_t>(clearValues.size());
}

void RenderCommandBuffer::BindResource(uint32_t set, Gfx::ShaderResource* resource)
{
    BindResourceCmd* ptr = cm.Push<BindResourceCmd>(&CommandStreamProcessor::BindResource);
    ptr->set = set;
    ptr->resource = resource;
}

void RenderCommandBuffer::BindResource(uint32_t set, const std::vector<Gfx::DynamicBinding>& bindings)
{
    Gfx::DynamicBinding* extraData = nullptr;
    BindDynamicBindingsCmd* ptr = cm.Push<BindDynamicBindingsCmd>(
        &CommandStreamProcessor::BindDynamicBindings,
        &extraData,
        sizeof(Gfx::DynamicBinding) * bindings.size()
    );

    if (!bindings.empty())
        memcpy((void*)extraData, bindings.data(), sizeof(Gfx::DynamicBinding) * bindings.size());

    ptr->bindings = extraData;
    ptr->count = static_cast<uint32_t>(bindings.size());
    ptr->set = set;
}

void RenderCommandBuffer::SetPushConstant(void* data)
{
    SetPushConstantCmd* ptr = cm.Push<SetPushConstantCmd>(&CommandStreamProcessor::SetPushConstant);
    ptr->data = data;
}

void RenderCommandBuffer::DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig)
{
    DrawMeshCmd* ptr = cm.Push<DrawMeshCmd>(&CommandStreamProcessor::DrawMesh);
    ptr->meshHandle = meshHandle;
    ptr->shaderProgram = shaderProgram;
    ptr->pipelineConfig = pipelineConfig;
}

void RenderCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    DrawCmd* ptr = cm.Push<DrawCmd>(&CommandStreamProcessor::Draw);
    ptr->vertexCount = vertexCount;
    ptr->instanceCount = instanceCount;
    ptr->firstVertex = firstVertex;
    ptr->firstInstance = firstInstance;
}

void RenderCommandBuffer::DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    DrawIndirectCmd* ptr = cm.Push<DrawIndirectCmd>(&CommandStreamProcessor::DrawIndirect);
    ptr->buffer = buffer;
    ptr->offset = offset;
    ptr->drawCount = drawCount;
    ptr->stride = stride;
}

void RenderCommandBuffer::DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    DrawIndexedIndirectCmd* ptr = cm.Push<DrawIndexedIndirectCmd>(&CommandStreamProcessor::DrawIndexedIndirect);
    ptr->buffer = buffer;
    ptr->offset = offset;
    ptr->drawCount = drawCount;
    ptr->stride = stride;
}

void RenderCommandBuffer::Blit(Gfx::ImageIdentifier src, Gfx::ImageIdentifier dst, Gfx::BlitOp blitOp)
{
    BlitCmd* ptr = cm.Push<BlitCmd>(&CommandStreamProcessor::Blit);
    ptr->src = src;
    ptr->dst = dst;
    ptr->op = blitOp;
}

void RenderCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    Rect2D* extraData = nullptr;
    SetScissorCmd* ptr = cm.Push<SetScissorCmd>(
        &CommandStreamProcessor::SetScissor,
        &extraData,
        sizeof(Rect2D) * scissorCount
    );

    if (rect && scissorCount > 0)
        memcpy(extraData, rect, sizeof(Rect2D) * scissorCount);

    ptr->firstScissor = firstScissor;
    ptr->scissorCount = scissorCount;
    ptr->rects = extraData;
}

void RenderCommandBuffer::SetViewport(const Gfx::Viewport& viewport)
{
    SetViewportCmd* ptr = cm.Push<SetViewportCmd>(&CommandStreamProcessor::SetViewport);
    ptr->viewport = viewport;
}

void RenderCommandBuffer::SetLineWidth(float lineWidth)
{
    SetLineWidthCmd* ptr = cm.Push<SetLineWidthCmd>(&CommandStreamProcessor::SetLineWidth);
    ptr->lineWidth = lineWidth;
}

void RenderCommandBuffer::SetDepthBias(float constantFactor, float clamp, float slopeFactor)
{
    SetDepthBiasCmd* ptr = cm.Push<SetDepthBiasCmd>(&CommandStreamProcessor::SetDepthBias);
    ptr->constantFactor = constantFactor;
    ptr->clamp = clamp;
    ptr->slopeFactor = slopeFactor;
}

void RenderCommandBuffer::SetDepthBiasEnable(bool enable)
{
    SetDepthBiasEnableCmd* ptr = cm.Push<SetDepthBiasEnableCmd>(&CommandStreamProcessor::SetDepthBiasEnable);
    ptr->enable = enable;
}

void RenderCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    DispatchCmd* ptr = cm.Push<DispatchCmd>(&CommandStreamProcessor::Dispatch);
    ptr->groupCountX = groupCountX;
    ptr->groupCountY = groupCountY;
    ptr->groupCountZ = groupCountZ;
}

void RenderCommandBuffer::DispatchIndirect(Gfx::Buffer* buffer, size_t bufferOffset)
{
    DispatchIndirectCmd* ptr = cm.Push<DispatchIndirectCmd>(&CommandStreamProcessor::DispatchIndirect);
    ptr->buffer = buffer;
    ptr->bufferOffset = bufferOffset;
}

RenderCommandBuffer::RenderCommandBuffer(RenderResourceAllocator* resourceAllocator)
    : cm(), resourceAllocator(resourceAllocator)
{
}
