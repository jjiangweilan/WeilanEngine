#include "VKCommandBuffer.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "GfxDriver/Vulkan/VKRenderPass.hpp"
#include "GfxDriver/Vulkan/VKShaderProgram.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "Libs/Assert.hpp"
#include "RHI/VKRenderGraph.hpp"
#include "VKBuffer.hpp"
#include "VKImage.hpp"
#include <_abort.h>

namespace Gfx
{

void VKCommandBuffer::BeginRenderPass(Gfx::RenderPass& renderPass, std::span<Gfx::ClearValue> clearValues)
{
    ASSERT(clearValues.size() <= 8);

    VKBeginRenderPassCmd cmd{};

    if (validationCheck && !renderPass.RenderPassRenderingValidationCheck())
    {
        spdlog::critical("failed to execute render pass because it's not ready for rendering, you need to set subpass and also set attachments in subpass");
    }

    cmd.renderPass = static_cast<VKRenderPass*>(&renderPass);
    for (int i = 0; i < clearValues.size() && i < 8; ++i)
    {
        memcpy(cmd.clearValues, clearValues.data(), clearValues.size() * sizeof(Gfx::ClearValue));
    }
    cmd.clearValueCount = clearValues.size();

    cmds.push_back(VKCmd{VKCmdType::BeginRenderPass, cmd});
}

void VKCommandBuffer::EndRenderPass()
{
    VKEndRenderPassCmd cmd{};
    cmds.push_back(VKCmd{VKCmdType::EndRenderPass, cmd});
}

void VKCommandBuffer::DrawIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    VKDrawIndirectCmd cmd{};

    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.drawCount = drawCount;
    cmd.stride = stride;

    cmds.push_back(VKCmd{VKCmdType::DrawIndirect, cmd});
}

void VKCommandBuffer::DrawIndexedIndirect(Gfx::Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride)
{
    VKDrawIndexedIndirectCmd cmd{};

    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.drawCount = drawCount;
    cmd.stride = stride;

    cmds.push_back(VKCmd{VKCmdType::DrawIndexedIndirect, cmd});
}

void VKCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    VKDrawCmd cmd{};

    cmd.vertexCount = vertexCount;
    cmd.instanceCount = instanceCount;
    cmd.firstVertex = firstVertex;
    cmd.firstInstance = firstInstance;

    cmds.push_back(VKCmd{VKCmdType::Draw, cmd});
}

void VKCommandBuffer::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
)
{
    VKDrawIndexedCmd cmd{};
    cmd.indexCount = indexCount;
    cmd.instanceCount = instanceCount;
    cmd.firstIndex = firstIndex;
    cmd.vertexOffset = vertexOffset;
    cmd.firstInstance = firstInstance;

    cmds.push_back(VKCmd{VKCmdType::DrawIndexed, cmd});
}

void VKCommandBuffer::BindResource(uint32_t set, Gfx::ShaderResource* resource)
{
    if (set > 4)
        return;

    VKBindResourceCmd cmd{};
    cmd.set = set;
    cmd.resource = static_cast<VKShaderResource*>(resource);

    cmds.push_back(VKCmd{VKCmdType::BindResource, cmd});
}

void VKCommandBuffer::BindShaderProgram(RefPtr<Gfx::ShaderProgram> bProgram, const PipelineConfig& config)
{
    VKBindShaderProgramCmd cmd{};

    cmd.program = (VKShaderProgram*)bProgram.Get();
    cmd.config = config;

    cmds.push_back(VKCmd{VKCmdType::BindShaderProgram, cmd});
}

void VKCommandBuffer::BindVertexBuffer(
    std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
)
{
    ASSERT(vertexBufferBindings.size() <= 8);

    VKBindVertexBufferCmd cmd{};
    for (int i = 0; i < vertexBufferBindings.size() && i < 8; ++i)
        cmd.vertexBufferBindings[i] = vertexBufferBindings[i];
    cmd.firstBindingIndex = firstBindingIndex;
    cmd.vertexBufferBindingCount = vertexBufferBindings.size();

    cmds.push_back(VKCmd{VKCmdType::BindVertexBuffer, cmd});
}

void VKCommandBuffer::BindIndexBuffer(RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType)
{
    VKBindIndexBufferCmd cmd{};

    cmd.buffer = static_cast<VKBuffer*>(buffer.Get());
    cmd.offset = offset;
    cmd.indexType = indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    cmds.push_back(VKCmd{VKCmdType::BindIndexBuffer, cmd});
}

void VKCommandBuffer::SetViewport(const Viewport& viewport)
{
    VKSetViewportCmd cmd{};
    VkViewport v{
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.width,
        .height = viewport.height,
        .minDepth = viewport.minDepth,
        .maxDepth = viewport.maxDepth
    };
    cmd.viewport = v;
    cmds.push_back(VKCmd{VKCmdType::SetViewport, cmd});
}

void VKCommandBuffer::CopyImageToBuffer(
    RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
)
{
    ASSERT(regions.size() < 8);
    VKCopyImageToBufferCmd cmd{};

    cmd.src = static_cast<VKImage*>(src.Get());
    cmd.dst = static_cast<VKBuffer*>(dst.Get());
    for (int i = 0; i < regions.size() && i < 8; ++i)
        cmd.regions[i] = regions[i];

    cmd.regionsCount = regions.size();

    cmds.push_back(VKCmd{VKCmdType::CopyImageToBuffer, cmd});
};

void VKCommandBuffer::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data)
{
    VKSetPushConstantCmd cmd{};
    cmd.shaderProgram = static_cast<VKShaderProgram*>(shaderProgram.Get());

    uint32_t totalSize = 0;
    cmd.stages = 0;
    for (const auto& ps : shaderProgram->GetShaderInfo().pushConstants)
    {
        cmd.stages |= MapShaderStages(ps.stages);
        totalSize += ps.size;
    }
    cmd.dataSize = totalSize;
    memcpy(cmd.data, data, totalSize < 128 ? totalSize : 128);
    cmds.push_back(VKCmd{VKCmdType::SetPushConstant, cmd});
};
void VKCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    ASSERT(scissorCount <= 8);

    VKSetScissorCmd cmd{};
    cmd.firstScissor = firstScissor;
    cmd.scissorCount = scissorCount;

    for (int i = 0; i < scissorCount; ++i)
    {
        cmd.rects[i].offset.x = rect->offset.x;
        cmd.rects[i].offset.y = rect->offset.y;
        cmd.rects[i].extent.width = rect->extent.width;
        cmd.rects[i].extent.height = rect->extent.height;
    }

    memcpy(cmd.rects, rect, scissorCount);
    cmds.push_back(VKCmd{VKCmdType::SetScissor, cmd});
};
void VKCommandBuffer::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VKDispatchCmd cmd{};
    cmd.groupCountX = groupCountX;
    cmd.groupCountY = groupCountY;
    cmd.groupCountZ = groupCountZ;

    cmds.push_back(VKCmd{VKCmdType::Dispatch, cmd});
};
void VKCommandBuffer::DispatchIndirect(Buffer* buffer, size_t bufferOffset)
{
    VKDispatchIndirectCmd cmd{};
    cmd.buffer = static_cast<VKBuffer*>(buffer);
    cmd.bufferOffset = bufferOffset;

    cmds.push_back(VKCmd{VKCmdType::DispatchIndirect, cmd});
};
void VKCommandBuffer::NextRenderPass()
{
    VKNextRenderPassCmd cmd{};

    cmds.push_back(VKCmd{VKCmdType::NextRenderPass, cmd});
};
void VKCommandBuffer::PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings)
{
    ASSERT(bindings.size() <= 8);
    VKPushDescriptorCmd cmd{};
    cmd.shader = static_cast<VKShaderProgram*>(&shader);
    cmd.set = set;
    cmd.bindingCount = bindings.size() <= 8 ? bindings.size() : 8;
    memcpy(cmd.bindings, bindings.data(), cmd.bindingCount * sizeof(DescriptorBinding));

    cmds.push_back({VKCmd{VKCmdType::PushDescriptorSet, cmd}});
};

void VKCommandBuffer::CopyBuffer(
    RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
)
{
    ASSERT(copyRegions.size() <= 8);
    VKCopyBufferCmd cmd{};
    cmd.src = static_cast<VKBuffer*>(bSrc.Get());
    cmd.dst = static_cast<VKBuffer*>(bDst.Get());
    cmd.copyRegionCount = copyRegions.size();
    for (int i = 0; i < copyRegions.size(); ++i)
    {
        auto& c = copyRegions[i];
        cmd.copyRegions[i] = {c.srcOffset, c.dstOffset, c.size};
    }
    cmds.push_back(VKCmd{VKCmdType::CopyBuffer, cmd});
};
void VKCommandBuffer::CopyBufferToImage(
    RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
)
{
    ASSERT(regions.size() < 8);
    VKCopyBufferToImageCmd cmd{};
    cmd.src = static_cast<VKBuffer*>(src.Get());
    cmd.dst = static_cast<VKImage*>(dst.Get());

    int i = 0;
    for (auto& r : regions)
    {
        VkBufferImageCopy region;
        region.bufferOffset = r.bufferOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = MapImageAspect(r.layers.aspectMask);
        region.imageSubresource.mipLevel = r.layers.mipLevel;
        region.imageSubresource.baseArrayLayer = r.layers.baseArrayLayer;
        region.imageSubresource.layerCount = r.layers.layerCount;
        region.imageOffset = VkOffset3D{r.offset.x, r.offset.y, r.offset.z};
        region.imageExtent = VkExtent3D{r.extend.width, r.extend.height, r.extend.depth};

        cmd.regions[i] = region;
        i += 1;
    }
    cmd.regionCount = regions.size();

    cmds.push_back(VKCmd{VKCmdType::CopyBufferToImage, cmd});
};

void VKCommandBuffer::Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp)
{
    VKBlitCmd cmd{};
    cmd.from = static_cast<VKImage*>(from.Get());
    cmd.to = static_cast<VKImage*>(to.Get());
    cmd.blitOp = blitOp;

    cmds.push_back(VKCmd{VKCmdType::Blit, cmd});
}

void VKCommandBuffer::PresentImage(VKImage* image)
{
    VKPresentCmd cmd{};
    cmd.image = image;
    cmds.push_back({VKCmdType::Present, cmd});
}

void VKCommandBuffer::SetTexture(
    ShaderBindingHandle handle, int index, RG::ImageIdentifier id, std::optional<ImageViewOption> imageViewOption
)
{
    if (id.GetType() == RG::ImageIdentifier::Type::Image)
    {
        VKSetTextureCmd cmd{};

        cmd.handle = handle;
        cmd.image = id.GetAsImage();
        cmd.index = index;
        cmd.imageViewOption = imageViewOption;

        cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
    }
    else if (id.GetType() == RG::ImageIdentifier::Type::Handle)
    {
        auto image = graph->GetImage(id.GetAsUUID());

        VKSetTextureCmd cmd{};

        cmd.handle = handle;
        cmd.image = image;
        cmd.index = index;
        cmd.imageViewOption = imageViewOption;

        cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
    }
}

void VKCommandBuffer::SetTexture(
    ShaderBindingHandle handle, int index, Gfx::Image& image, std::optional<ImageViewOption> imageViewOption
)
{
    VKSetTextureCmd cmd{};

    cmd.handle = handle;
    cmd.image = &image;
    cmd.index = index;
    cmd.imageViewOption = imageViewOption;

    cmds.push_back(VKCmd{VKCmdType::SetTexture, cmd});
}

void VKCommandBuffer::SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer& buffer)
{
    VKSetBufferCmd cmd{};

    cmd.buffer = static_cast<VKBuffer*>(&buffer);
    cmd.handle = handle;
    cmd.index = index;

    cmds.push_back(VKCmd{VKCmdType::SetBuffer, cmd});
}

void VKCommandBuffer::AllocateAttachment(const RG::ImageIdentifier& id, RG::ImageDescription& desc)
{
    graph->Request(id, desc);
}

void VKCommandBuffer::BeginRenderPass(RG::RenderPass& renderPass, std::span<ClearValue> clearValues)
{
    VKRGBeginRenderPassCmd cmd{};

    if (validationCheck)
    {
        if (renderPass.GetAttachments().size() != clearValues.size() || !renderPass.IsValidForRendering())
        {
            throw std::runtime_error("");
        }
    }

    cmd.renderPass = &renderPass;
    int copySize = clearValues.size() <= 8 ? clearValues.size_bytes() : 8 * sizeof(ClearValue);
    memcpy(cmd.clearValues, clearValues.data(), copySize);
    cmd.clearValueCount = clearValues.size() <= 8 ? clearValues.size() : 8;

    cmds.push_back(VKCmd{VKCmdType::RGBeginRenderPass, cmd});
}

void VKCommandBuffer::Blit(RG::ImageIdentifier src, RG::ImageIdentifier dst, BlitOp blitOp)
{
    VKBlitCmd cmd{};

    VKImage* from = nullptr;
    VKImage* to = nullptr;
    if (src.GetType() == RG::ImageIdentifier::Type::Image)
    {
        from = static_cast<VKImage*>(src.GetAsImage());
    }
    else if (src.GetType() == RG::ImageIdentifier::Type::Handle)
    {
        from = graph->GetImage(src.GetAsUUID());
    }

    if (dst.GetType() == RG::ImageIdentifier::Type::Image)
    {
        to = static_cast<VKImage*>(dst.GetAsImage());
    }
    else if (src.GetType() == RG::ImageIdentifier::Type::Handle)
    {
        to = graph->GetImage(dst.GetAsUUID());
    }

    cmd.from = static_cast<VKImage*>(from);
    cmd.to = static_cast<VKImage*>(to);
    cmd.blitOp = blitOp;

    cmds.push_back(VKCmd{VKCmdType::Blit, cmd});
}

void VKCommandBuffer::BeginLabel(std::string_view label, float color[4])
{
    VKBeginLabelCmd cmd{};

    char* tmp = tmpMemory.Allocate<char>(label.size() + 1);
    strcpy(tmp, (char*)label.data());
    cmd.label = tmp;
    memcpy(cmd.color, color, sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::BeginLabel, cmd});
}
void VKCommandBuffer::EndLabel()
{
    VKCmd cmd{VKCmdType::EndLabel};

    cmds.push_back(cmd);
}
void VKCommandBuffer::InsertLabel(std::string_view label, float color[4])
{
    VKInsertLabelCmd cmd{};

    char* tmp = tmpMemory.Allocate<char>(label.size() + 1);
    strcpy(tmp, (char*)label.data());
    cmd.label = tmp;
    memcpy(cmd.color, color, sizeof(float) * 4);

    cmds.push_back(VKCmd{VKCmdType::InsertLabel, cmd});
}

void VKCommandBuffer::SetLineWidth(float lineWidth)
{
    VKSetLineWidthCmd cmd{};

    cmd.lineWidth = lineWidth;

    cmds.push_back(VKCmd{VKCmdType::SetLineWidth, cmd});
}

std::shared_ptr<AsyncReadbackHandle> VKCommandBuffer::AsyncReadback(
    Gfx::Buffer& buffer, void* dst, size_t size, size_t offset
)
{
    VKAsyncReadbackCmd cmd{};

    std::shared_ptr<AsyncReadbackHandle> handle = std::make_shared<AsyncReadbackHandle>();
    cmd.buffer = static_cast<VKBuffer*>(&buffer);
    cmd.dst = dst;
    cmd.size = size;
    cmd.offset = offset;
    readbacks.push_back(handle);
    cmd.handle = &readbacks.back();

    cmds.push_back(VKCmd{VKCmdType::AsyncReadback, cmd});
    return handle;
}

} // namespace Gfx
