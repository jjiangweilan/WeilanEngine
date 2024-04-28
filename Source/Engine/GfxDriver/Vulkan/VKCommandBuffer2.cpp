#include "VKCommandBuffer2.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "GfxDriver/Vulkan/VKShaderProgram.hpp"
#include "GfxDriver/Vulkan/VKShaderResource.hpp"
#include "RHI/VKRenderGraph.hpp"
#include "VKBuffer.hpp"
#include "VKImage.hpp"

namespace Gfx
{

void VKCommandBuffer2::BeginRenderPass(Gfx::RenderPass& renderPass, const std::vector<Gfx::ClearValue>& clearValues)
{
    assert(clearValues.size() <= 8);

    VKCmd cmd{VKCmdType::BeginRenderPass};

    cmd.type = VKCmdType::BeginRenderPass;
    cmd.beginRenderPass.renderPass = static_cast<VKRenderPass*>(&renderPass);
    for (int i = 0; i < clearValues.size() && i < 8; ++i)
    {
        memcpy(cmd.beginRenderPass.clearValues, clearValues.data(), clearValues.size() * sizeof(Gfx::ClearValue));
    }
    cmd.beginRenderPass.clearValueCount = clearValues.size();

    cmds.push_back(cmd);
}

void VKCommandBuffer2::EndRenderPass()
{
    VKCmd cmd{VKCmdType::EndRenderPass};
    cmds.push_back(cmd);
}

void VKCommandBuffer2::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    VKCmd cmd{VKCmdType::Draw};

    cmd.draw.vertexCount = vertexCount;
    cmd.draw.instanceCount = instanceCount;
    cmd.draw.firstVertex = firstVertex;
    cmd.draw.firstInstance = firstInstance;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
)
{
    VKCmd cmd{VKCmdType::DrawIndexed};
    cmd.drawIndexed.indexCount = indexCount;
    cmd.drawIndexed.instanceCount = instanceCount;
    cmd.drawIndexed.firstIndex = firstIndex;
    cmd.drawIndexed.vertexOffset = vertexOffset;
    cmd.drawIndexed.firstInstance = firstInstance;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindResource(uint32_t set, Gfx::ShaderResource* resource)
{
    if (set > 4)
        return;

    VKCmd cmd{VKCmdType::BindResource};
    cmd.bindResource.set = set;
    cmd.bindResource.resource = static_cast<VKShaderResource*>(resource);

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindShaderProgram(RefPtr<Gfx::ShaderProgram> bProgram, const ShaderConfig& config)
{
    VKCmd cmd{VKCmdType::BindShaderProgram};
    cmd.bindShaderProgram.program = (VKShaderProgram*)bProgram.Get();
    cmd.bindShaderProgram.config = &config;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindVertexBuffer(
    std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
)
{
    assert(vertexBufferBindings.size() <= 8);

    VKCmd cmd{VKCmdType::BindVertexBuffer};
    for (int i = 0; i < vertexBufferBindings.size() && i < 8; ++i)
        cmd.bindVertexBuffer.vertexBufferBindings[i] = vertexBufferBindings[i];
    cmd.bindVertexBuffer.firstBindingIndex = firstBindingIndex;
    cmd.bindVertexBuffer.vertexBufferBindingCount = vertexBufferBindings.size();

    cmds.push_back(cmd);
}

void VKCommandBuffer2::BindIndexBuffer(
    RefPtr<Gfx::Buffer> buffer, uint64_t offset, Gfx::IndexBufferType indexBufferType
)
{
    VKCmd cmd{VKCmdType::BindIndexBuffer};

    cmd.bindIndexBuffer.buffer = static_cast<VKBuffer*>(buffer.Get());
    cmd.bindIndexBuffer.offset = offset;
    cmd.bindIndexBuffer.indexType =
        indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::SetViewport(const Viewport& viewport)
{
    VKCmd cmd{VKCmdType::SetViewport};
    VkViewport v{
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.width,
        .height = viewport.height,
        .minDepth = viewport.minDepth,
        .maxDepth = viewport.maxDepth
    };
    cmd.setViewport.viewport = v;
    cmds.push_back(cmd);
}

void VKCommandBuffer2::CopyImageToBuffer(
    RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
)
{
    assert(regions.size() < 8);
    VKCmd cmd{VKCmdType::CopyImageToBuffer};

    cmd.copyImageToBuffer.src = static_cast<VKImage*>(src.Get());
    cmd.copyImageToBuffer.dst = static_cast<VKBuffer*>(dst.Get());
    for (int i = 0; i < regions.size() && i < 8; ++i)
        cmd.copyImageToBuffer.regions[i] = regions[i];
};

void VKCommandBuffer2::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram, void* data)
{
    VKCmd cmd{VKCmdType::SetPushConstant};
    cmd.setPushConstant.shaderProgram = static_cast<VKShaderProgram*>(shaderProgram.Get());

    uint32_t totalSize = 0;
    cmd.setPushConstant.stages = 0;
    for (auto& ps : shaderProgram->GetShaderInfo().pushConstants)
    {
        auto& pushConstant = ps.second;
        cmd.setPushConstant.stages |= ShaderInfo::Utils::MapShaderStage(pushConstant.stages);
        totalSize += pushConstant.data.size;
    }
    cmd.setPushConstant.dataSize = totalSize;
    memcpy(cmd.setPushConstant.data, data, totalSize < 128 ? totalSize : 128);
    cmds.push_back(cmd);
};
void VKCommandBuffer2::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    assert(scissorCount <= 8);
    VKCmd cmd{VKCmdType::SetScissor};
    cmd.setScissor.firstScissor = firstScissor;
    cmd.setScissor.scissorCount = scissorCount;

    for (int i = 0; i < scissorCount; ++i)
    {
        cmd.setScissor.rects[i].offset.x = rect->offset.x;
        cmd.setScissor.rects[i].offset.y = rect->offset.y;
        cmd.setScissor.rects[i].extent.width = rect->extent.width;
        cmd.setScissor.rects[i].extent.height = rect->extent.height;
    }

    memcpy(cmd.setScissor.rects, rect, scissorCount);
    cmds.push_back(cmd);
};
void VKCommandBuffer2::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    VKCmd cmd{VKCmdType::Dispatch};
    cmd.dispatch.groupCountX = groupCountX;
    cmd.dispatch.groupCountY = groupCountY;
    cmd.dispatch.groupCountZ = groupCountZ;

    cmds.push_back(cmd);
};
void VKCommandBuffer2::DispatchIndir(Buffer* buffer, size_t bufferOffset)
{
    VKCmd cmd{VKCmdType::DispatchIndir};
    cmd.dispatchIndir.buffer = static_cast<VKBuffer*>(buffer);
    cmd.dispatchIndir.bufferOffset = bufferOffset;

    cmds.push_back(cmd);
};
void VKCommandBuffer2::NextRenderPass()
{
    VKCmd cmd{VKCmdType::NextRenderPass};
    cmds.push_back(cmd);
};
void VKCommandBuffer2::PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings)
{
    assert(bindings.size() <= 8);
    VKCmd cmd{VKCmdType::PushDescriptorSet};
    cmd.pushDescriptor.shader = static_cast<VKShaderProgram*>(&shader);
    cmd.pushDescriptor.set = set;
    cmd.pushDescriptor.bindingCount = bindings.size() <= 8 ? bindings.size() : 8;
    memcpy(cmd.pushDescriptor.bindings, bindings.data(), cmd.pushDescriptor.bindingCount * sizeof(DescriptorBinding));
    cmds.push_back(cmd);
};

void VKCommandBuffer2::CopyBuffer(
    RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
)
{
    assert(copyRegions.size() <= 8);
    VKCmd cmd{VKCmdType::CopyBuffer};
    cmd.copyBuffer.src = static_cast<VKBuffer*>(bSrc.Get());
    cmd.copyBuffer.dst = static_cast<VKBuffer*>(bDst.Get());
    cmd.copyBuffer.copyRegionCount = copyRegions.size();
    for (int i = 0; i < copyRegions.size(); ++i)
    {
        auto& c = copyRegions[i];
        cmd.copyBuffer.copyRegions[i] = {c.srcOffset, c.dstOffset, c.size};
    }
    cmds.push_back(cmd);
};
void VKCommandBuffer2::CopyBufferToImage(
    RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
)
{
    assert(regions.size() < 8);
    VKCmd cmd{VKCmdType::CopyBufferToImage};
    cmd.copyBufferToImage.src = static_cast<VKBuffer*>(src.Get());
    cmd.copyBufferToImage.dst = static_cast<VKImage*>(dst.Get());

    int i = 0;
    for (auto& r : regions)
    {
        VkBufferImageCopy region;
        region.bufferOffset = r.srcOffset;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = MapImageAspect(r.layers.aspectMask);
        region.imageSubresource.mipLevel = r.layers.mipLevel;
        region.imageSubresource.baseArrayLayer = r.layers.baseArrayLayer;
        region.imageSubresource.layerCount = r.layers.layerCount;
        region.imageOffset = VkOffset3D{r.offset.x, r.offset.y, r.offset.z};
        region.imageExtent = VkExtent3D{r.extend.width, r.extend.height, r.extend.depth};

        cmd.copyBufferToImage.regions[i] = region;
        i += 1;
    }
    cmd.copyBufferToImage.regionCount = regions.size();

    cmds.push_back(cmd);
};

void VKCommandBuffer2::Blit(RefPtr<Gfx::Image> from, RefPtr<Gfx::Image> to, BlitOp blitOp)
{
    VKCmd cmd{VKCmdType::Blit};
    cmd.blit.from = static_cast<VKImage*>(from.Get());
    cmd.blit.to = static_cast<VKImage*>(to.Get());
    cmd.blit.blitOp = blitOp;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::PresentImage(VKImage* image)
{
    VKCmd cmd{VKCmdType::Present};
    cmd.present.image = image;
    cmds.push_back(cmd);
}

void VKCommandBuffer2::SetTexture(ShaderBindingHandle handle, int index, RG::ImageIdentifier id)
{
    if (id.GetType() == RG::ImageIdentifier::Type::Image)
    {
        VKCmd cmd{VKCmdType::SetTexture};

        cmd.setTexture.handle = handle;
        cmd.setTexture.image = id.GetAsImage();
        cmd.setTexture.index = index;

        cmds.push_back(cmd);
    }
    else if (id.GetType() == RG::ImageIdentifier::Type::Handle)
    {
        auto image = graph->GetImage(id.GetAsUUID());

        VKCmd cmd{VKCmdType::SetTexture};

        cmd.setTexture.handle = handle;
        cmd.setTexture.image = image;
        cmd.setTexture.index = index;

        cmds.push_back(cmd);
    }
}

void VKCommandBuffer2::SetTexture(ShaderBindingHandle handle, int index, Gfx::Image& image)
{
    VKCmd cmd{VKCmdType::SetTexture};

    cmd.setTexture.handle = handle;
    cmd.setTexture.image = &image;
    cmd.setTexture.index = index;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::SetBuffer(ShaderBindingHandle handle, int index, Gfx::Buffer& buffer)
{
    VKCmd cmd{VKCmdType::SetBuffer};

    cmd.setBuffer.buffer = static_cast<VKBuffer*>(&buffer);
    cmd.setBuffer.handle = handle;
    cmd.setTexture.index = index;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::AllocateAttachment(RG::ImageIdentifier& id, RG::ImageDescription& desc)
{
    graph->Request(id, desc);
}

void VKCommandBuffer2::BeginRenderPass(RG::RenderPass& renderPass, std::span<ClearValue> clearValues)
{
    VKCmd cmd{VKCmdType::RGBeginRenderPass};

    cmd.rgBeginRenderPass.renderPass = &renderPass;
    int copySize = clearValues.size() <= 8 ? clearValues.size_bytes() : 8 * sizeof(ClearValue);
    memcpy(cmd.rgBeginRenderPass.clearValues, clearValues.data(), copySize);
    cmd.rgBeginRenderPass.clearValueCount = clearValues.size() <= 8 ? clearValues.size() : 8;

    cmds.push_back(cmd);
}

void VKCommandBuffer2::Blit(RG::ImageIdentifier src, RG::ImageIdentifier dst, BlitOp blitOp)
{
    VKCmd cmd{VKCmdType::Blit};

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

    cmd.blit.from = static_cast<VKImage*>(from);
    cmd.blit.to = static_cast<VKImage*>(to);
    cmd.blit.blitOp = blitOp;

    cmds.push_back(cmd);
}
} // namespace Gfx
