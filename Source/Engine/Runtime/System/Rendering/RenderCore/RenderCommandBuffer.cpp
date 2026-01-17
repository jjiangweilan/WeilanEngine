#include "RenderCommandBuffer.hpp"

void RenderCommandBuffer::AllocateTempImage(const RenderImageDescriptor& desc, ImageIdentifier& id)
{
    resourceAllocator->AllocateTempImage(desc, id);
}

void RenderCommandBuffer::BeginLabel(std::string_view label)
{
    char* extraData = nullptr;
    BeginLabelCmd* ptr = cm.Push<BeginLabelCmd>(
        &BeginLabelCmd::Execute,
        &extraData,
        label.length() + 1
    );

    memcpy(extraData, label.data(), label.length() + 1);
    ptr->str = extraData;
}

void RenderCommandBuffer::EndLabel()
{
    cm.Push<EndLabelCmd>(&EndLabelCmd::Execute);
}

void RenderCommandBuffer::SetRenderPass(std::span<const RenderAttachment> images)
{
    RenderAttachment* extraData = nullptr;
    SetRenderPassCmd* ptr = cm.Push<SetRenderPassCmd>(
        &SetRenderPassCmd::Execute,
        &extraData,
        sizeof(RenderAttachment) * images.size()
    );

    if (!images.empty())
        memcpy((void*)extraData, images.data(), sizeof(RenderAttachment) * images.size());

    ptr->attachments = extraData;
    ptr->count = static_cast<uint32_t>(images.size());
}

void RenderCommandBuffer::SetClearValues(std::span<Gfx::ClearValue> clearValues)
{
    Gfx::ClearValue* extraData = nullptr;
    SetClearValuesCmd* ptr = cm.Push<SetClearValuesCmd>(
        &SetClearValuesCmd::Execute,
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
    BindResourceCmd* ptr = cm.Push<BindResourceCmd>(&BindResourceCmd::Execute);
    ptr->set = set;
    ptr->resource = resource;
}

void RenderCommandBuffer::BindResource(uint32_t set, const std::vector<DynamicBinding>& bindings)
{
    DynamicBinding* extraData = nullptr;
    BindDynamicBindingsCmd* ptr = cm.Push<BindDynamicBindingsCmd>(
        &BindDynamicBindingsCmd::Execute,
        &extraData,
        sizeof(DynamicBinding) * bindings.size()
    );

    if (!bindings.empty())
        memcpy((void*)extraData, bindings.data(), sizeof(DynamicBinding) * bindings.size());

    ptr->bindings = extraData;
    ptr->count = static_cast<uint32_t>(bindings.size());
    ptr->set = set;
}

void RenderCommandBuffer::SetPushConstant(void* data)
{
    SetPushConstantCmd* ptr = cm.Push<SetPushConstantCmd>(&SetPushConstantCmd::Execute);
    ptr->data = data;
}

void RenderCommandBuffer::DrawMesh(const MeshHandle& meshHandle, Gfx::ShaderProgram* shaderProgram, const Gfx::PipelineConfig& pipelineConfig)
{
    DrawMeshCmd* ptr = cm.Push<DrawMeshCmd>(&DrawMeshCmd::Execute);
    ptr->meshHandle = meshHandle;
    ptr->shaderProgram = shaderProgram;
    ptr->pipelineConfig = pipelineConfig;
}

void RenderCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    DrawCmd* ptr = cm.Push<DrawCmd>(&DrawCmd::Execute);
    ptr->vertexCount = vertexCount;
    ptr->instanceCount = instanceCount;
    ptr->firstVertex = firstVertex;
    ptr->firstInstance = firstInstance;
}

void RenderCommandBuffer::DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    DrawIndirectCmd* ptr = cm.Push<DrawIndirectCmd>(&DrawIndirectCmd::Execute);
    ptr->buffer = buffer;
    ptr->offset = offset;
    ptr->drawCount = drawCount;
    ptr->stride = stride;
}

void RenderCommandBuffer::DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    DrawIndexedIndirectCmd* ptr = cm.Push<DrawIndexedIndirectCmd>(&DrawIndexedIndirectCmd::Execute);
    ptr->buffer = buffer;
    ptr->offset = offset;
    ptr->drawCount = drawCount;
    ptr->stride = stride;
}

void RenderCommandBuffer::Blit(ImageIdentifier src, ImageIdentifier dst, BlitOp blitOp)
{
    BlitCmd* ptr = cm.Push<BlitCmd>(&BlitCmd::Execute);
    ptr->src = src;
    ptr->dst = dst;
    ptr->op = blitOp;
}

void RenderCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    Rect2D* extraData = nullptr;
    SetScissorCmd* ptr = cm.Push<SetScissorCmd>(
        &SetScissorCmd::Execute,
        &extraData,
        sizeof(Rect2D) * scissorCount
    );

    if (rect && scissorCount > 0)
        memcpy(extraData, rect, sizeof(Rect2D) * scissorCount);

    ptr->firstScissor = firstScissor;
    ptr->scissorCount = scissorCount;
    ptr->rects = extraData;
}

void RenderCommandBuffer::SetViewport(const Viewport& viewport)
{
    SetViewportCmd* ptr = cm.Push<SetViewportCmd>(&SetViewportCmd::Execute);
    ptr->viewport = viewport;
}

void RenderCommandBuffer::SetLineWidth(float lineWidth)
{
    SetLineWidthCmd* ptr = cm.Push<SetLineWidthCmd>(&SetLineWidthCmd::Execute);
    ptr->lineWidth = lineWidth;
}

void RenderCommandBuffer::SetDepthBias(float constantFactor, float clamp, float slopeFactor)
{
    SetDepthBiasCmd* ptr = cm.Push<SetDepthBiasCmd>(&SetDepthBiasCmd::Execute);
    ptr->constantFactor = constantFactor;
    ptr->clamp = clamp;
    ptr->slopeFactor = slopeFactor;
}

void RenderCommandBuffer::SetDepthBiasEnable(bool enable)
{
    SetDepthBiasEnableCmd* ptr = cm.Push<SetDepthBiasEnableCmd>(&SetDepthBiasEnableCmd::Execute);
    ptr->enable = enable;
}

void RenderCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    DispatchCmd* ptr = cm.Push<DispatchCmd>(&DispatchCmd::Execute);
    ptr->groupCountX = groupCountX;
    ptr->groupCountY = groupCountY;
    ptr->groupCountZ = groupCountZ;
}

void RenderCommandBuffer::DispatchIndirect(Gfx::Buffer* buffer, size_t bufferOffset)
{
    DispatchIndirectCmd* ptr = cm.Push<DispatchIndirectCmd>(&DispatchIndirectCmd::Execute);
    ptr->buffer = buffer;
    ptr->bufferOffset = bufferOffset;
}
