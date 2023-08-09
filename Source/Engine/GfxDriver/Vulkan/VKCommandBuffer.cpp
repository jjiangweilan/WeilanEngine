#include "VKCommandBuffer.hpp"
#include "Internal/VKDevice.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKExtensionFunc.hpp"
#include "VKFrameBuffer.hpp"
#include "VKRenderTarget.hpp"
#include "VKShaderProgram.hpp"
#include "VKShaderResource.hpp"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#undef MemoryBarrier
#endif

namespace Engine::Gfx
{

VKCommandBuffer::VKCommandBuffer(VkCommandBuffer vkCmdBuf) : vkCmdBuf(vkCmdBuf) {}

VKCommandBuffer::~VKCommandBuffer() {}

void VKCommandBuffer::BeginRenderPass(Gfx::RenderPass& renderPass, const std::vector<Gfx::ClearValue>& clearValues)
{
    Gfx::VKRenderPass& vRenderPass = static_cast<Gfx::VKRenderPass&>(renderPass);
    VkRenderPass vkRenderPass = vRenderPass.GetHandle();
    currentRenderPass = &vRenderPass;
    currentRenderIndex = 0;

    // framebuffer has to get inside the execution function due to how
    // RenderPass handle swapchain image as framebuffer attachment
    VkFramebuffer vkFramebuffer = vRenderPass.GetFrameBuffer();

    auto extent = vRenderPass.GetExtent();
    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = VK_NULL_HANDLE;
    renderPassBeginInfo.renderPass = vkRenderPass;
    renderPassBeginInfo.framebuffer = vkFramebuffer;
    renderPassBeginInfo.renderArea = {{0, 0}, {extent.width, extent.height}};
    renderPassBeginInfo.clearValueCount = clearValues.size();
    VkClearValue vkClearValues[16];
    int i = 0;
    for (auto& v : clearValues)
    {
        memcpy(&vkClearValues[i].color, &v.color, sizeof(v.color));
        vkClearValues[i].depthStencil.stencil = v.depthStencil.stencil;
        vkClearValues[i].depthStencil.depth = v.depthStencil.depth;
        ++i;
    }
    renderPassBeginInfo.pClearValues = vkClearValues;

    vkCmdBeginRenderPass(vkCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VKCommandBuffer::EndRenderPass()
{
    vkCmdEndRenderPass(vkCmdBuf);
    currentRenderPass = nullptr;
}

void VKCommandBuffer::SetViewport(const Viewport& viewport)
{
    VkViewport v{
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.width,
        .height = viewport.height,
        .minDepth = viewport.minDepth,
        .maxDepth = viewport.maxDepth};

    vkCmdSetViewport(vkCmdBuf, 0, 1, &v);
}

void VKCommandBuffer::PushDescriptor(ShaderProgram& shader, uint32_t set, std::span<DescriptorBinding> bindings)
{
    const int writeDescriptorSetSize = 32;
    assert(bindings.size() < writeDescriptorSetSize);

    VKShaderProgram& vkShader = static_cast<VKShaderProgram&>(shader);
    VkWriteDescriptorSet writeSets[writeDescriptorSetSize];
    VkDescriptorImageInfo imageInfos[writeDescriptorSetSize * 2];

    int i = 0;
    int imageIndex = 0;
    for (auto& b : bindings)
    {
        if (b.image != nullptr)
        {
            VkDescriptorImageInfo* p = imageInfos + imageIndex;
            for (int imageJedex = 0; imageJedex < b.descriptorCount; imageJedex += 1, imageIndex += 1)
            {
                VKImage* vkImage = static_cast<VKImage*>(b.image + imageJedex);
                auto layout = vkImage->GetLayout();
                auto imageView = vkImage->GetDefaultVkImageView();
                imageInfos[imageIndex] = {.imageView = imageView, .imageLayout = layout};
            }

            writeSets[i] = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = VK_NULL_HANDLE,
                .dstBinding = b.dstBinding,
                .dstArrayElement = b.dstArrayElement,
                .descriptorCount = b.descriptorCount,
                .descriptorType =
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // I think should be queried from shader, but shader
                                                               // currently don't support an easy way to get the binding
                                                               // type (need iterate through ShaderInfo.bindings)
                .pImageInfo = imageInfos,
            };
        }

        i += 1;
    }

    VKExtensionFunc::vkCmdPushDescriptorSetKHR(
        vkCmdBuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        vkShader.GetVKPipelineLayout(),
        set,
        bindings.size(),
        writeSets
    );
}

void VKCommandBuffer::Blit(RefPtr<Gfx::Image> bFrom, RefPtr<Gfx::Image> bTo, BlitOp blitOp)
{
    VKImage* from = static_cast<VKImage*>(bFrom.Get());
    VKImage* to = static_cast<VKImage*>(bTo.Get());

    uint32_t srcMip = blitOp.srcMip.value_or(0);
    uint32_t dstMip = blitOp.dstMip.value_or(0);
    VkImageBlit blit;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {
        (int32_t)(to->GetDescription().width / glm::pow(2, dstMip)),
        (int32_t)(to->GetDescription().height / glm::pow(2, dstMip)),
        1};
    VkImageSubresourceLayers dstLayers;
    dstLayers.aspectMask = to->GetDefaultSubresourceRange().aspectMask;
    dstLayers.baseArrayLayer = 0;
    dstLayers.layerCount = to->GetDefaultSubresourceRange().layerCount;
    dstLayers.mipLevel = dstMip;
    blit.dstSubresource = dstLayers;

    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {
        (int32_t)(from->GetDescription().width / glm::pow(2, srcMip)),
        (int32_t)(from->GetDescription().height / glm::pow(2, srcMip)),
        1};
    VkImageSubresourceLayers srcLayers = dstLayers;
    srcLayers.mipLevel = blitOp.srcMip.value_or(0);
    blit.srcSubresource = srcLayers; // basically copy the resources from dst
                                     // without much configuration

    vkCmdBlitImage(
        vkCmdBuf,
        from->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        to->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        VK_FILTER_NEAREST
    );
}

void VKCommandBuffer::BindResource(RefPtr<Gfx::ShaderResource> resource_)
{
    VKShaderResource* resource = (VKShaderResource*)resource_.Get();

    VkDescriptorSet descSet = resource->GetDescriptorSet();
    if (descSet != VK_NULL_HANDLE)
        vkCmdBindDescriptorSets(
            vkCmdBuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            ((VKShaderProgram*)resource->GetShader().Get())->GetVKPipelineLayout(),
            resource->GetDescriptorSetSlot(),
            1,
            &descSet,
            0,
            VK_NULL_HANDLE
        );
}

void VKCommandBuffer::BindShaderProgram(RefPtr<Gfx::ShaderProgram> bProgram, const ShaderConfig& config)
{
    assert(currentRenderPass != nullptr);
    VKShaderProgram* program = static_cast<VKShaderProgram*>(bProgram.Get());

    // binding pipeline
    auto pipeline = program->RequestPipeline(config, currentRenderPass->GetHandle(), currentRenderIndex);
    vkCmdBindPipeline(vkCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void VKCommandBuffer::SetScissor(uint32_t firstScissor, uint32_t scissorCount, Rect2D* rect)
{
    assert(scissorCount < 8);

    VkRect2D vkRects[8];
    for (uint32_t i = 0; i < scissorCount; ++i)
    {
        vkRects[i].offset.x = rect->offset.x;
        vkRects[i].offset.y = rect->offset.y;
        vkRects[i].extent.width = rect->extent.width;
        vkRects[i].extent.height = rect->extent.height;
    }

    vkCmdSetScissor(vkCmdBuf, firstScissor, scissorCount, vkRects);
}

void VKCommandBuffer::BindVertexBuffer(
    std::span<const VertexBufferBinding> vertexBufferBindings, uint32_t firstBindingIndex
)
{
    assert(vertexBufferBindings.size() <= 16);
    VkBuffer vkBuffers[16];
    uint64_t vkOffsets[16];
    for (uint32_t i = 0; i < vertexBufferBindings.size(); ++i)
    {
        VKBuffer* vkbuf = static_cast<VKBuffer*>(vertexBufferBindings[i].buffer);
        vkBuffers[i] = vkbuf->GetHandle();
        vkOffsets[i] = vertexBufferBindings[i].offset;
    }

    vkCmdBindVertexBuffers(vkCmdBuf, firstBindingIndex, vertexBufferBindings.size(), vkBuffers, vkOffsets);
}

void VKCommandBuffer::BindIndexBuffer(
    RefPtr<Gfx::Buffer> bBuffer, uint64_t offset, Gfx::IndexBufferType indexBufferType
)
{
    VKBuffer* buffer = static_cast<VKBuffer*>(bBuffer.Get());
    VkIndexType indexType =
        indexBufferType == Gfx::IndexBufferType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    VkBuffer indexBuf = buffer->GetHandle();
    vkCmdBindIndexBuffer(vkCmdBuf, indexBuf, offset, indexType);
}

void VKCommandBuffer::DrawIndexed(
    uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance
)
{
    vkCmdDrawIndexed(vkCmdBuf, indexCount, instanceCount, firstIndex, vertexOffset, firstIndex);
}

void VKCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    vkCmdDraw(vkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VKCommandBuffer::NextRenderPass()
{
    currentRenderIndex += 1;
    vkCmdNextSubpass(vkCmdBuf, VK_SUBPASS_CONTENTS_INLINE);
}

void VKCommandBuffer::SetPushConstant(RefPtr<Gfx::ShaderProgram> shaderProgram_, void* data)
{
    VKShaderProgram* shaderProgram = static_cast<VKShaderProgram*>(shaderProgram_.Get());

    VkShaderStageFlags stages = 0;
    uint32_t totalSize = 0;
    for (auto& ps : shaderProgram->GetShaderInfo().pushConstants)
    {
        auto& pushConstant = ps.second;
        stages |= ShaderInfo::Utils::MapShaderStage(pushConstant.stages);
        totalSize += pushConstant.data.size;
    }

    vkCmdPushConstants(vkCmdBuf, shaderProgram->GetVKPipelineLayout(), stages, 0, totalSize, data);
}

void VKCommandBuffer::CopyBuffer(
    RefPtr<Gfx::Buffer> bSrc, RefPtr<Gfx::Buffer> bDst, std::span<BufferCopyRegion> copyRegions
)
{
    VKBuffer* src = static_cast<VKBuffer*>(bSrc.Get());
    VKBuffer* dst = static_cast<VKBuffer*>(bDst.Get());

    std::vector<VkBufferCopy> regions;
    for (auto& c : copyRegions)
    {
        regions.push_back({c.srcOffset, c.dstOffset, c.size});
    }

    vkCmdCopyBuffer(vkCmdBuf, src->GetHandle(), dst->GetHandle(), regions.size(), regions.data());
}

void VKCommandBuffer::CopyBufferToImage(
    RefPtr<Gfx::Buffer> src, RefPtr<Gfx::Image> dst, std::span<BufferImageCopyRegion> regions
)
{
    assert(!regions.empty());
    auto image = static_cast<VKImage*>(dst.Get());
    auto stageBuf = static_cast<VKBuffer*>(src.Get());

    std::vector<VkBufferImageCopy> vkRegions;

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

        vkRegions.push_back(region);
    }

    vkCmdCopyBufferToImage(
        vkCmdBuf,
        stageBuf->GetHandle(),
        image->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        vkRegions.size(),
        vkRegions.data()
    );
}

void VKCommandBuffer::Begin()
{
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr};

    vkBeginCommandBuffer(vkCmdBuf, &beginInfo);
}

void VKCommandBuffer::End()
{
    vkEndCommandBuffer(vkCmdBuf);
}

void VKCommandBuffer::Barrier(GPUBarrier* barriers, uint32_t barrierCount)
{
    for (int i = 0; i < barrierCount; ++i)
    {
        const GPUBarrier& barrier = barriers[i];
        if (barrier.buffer != nullptr)
        {
            auto buffer = static_cast<VKBuffer*>(barrier.buffer.Get());

            VkBufferMemoryBarrier memoryBarrier;
            memoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            memoryBarrier.pNext = VK_NULL_HANDLE;
            memoryBarrier.srcAccessMask = MapAccessMask(barrier.srcAccessMask);
            memoryBarrier.dstAccessMask = MapAccessMask(barrier.dstAccessMask);
            memoryBarrier.srcQueueFamilyIndex = barrier.bufferInfo.srcQueueFamilyIndex;
            memoryBarrier.dstQueueFamilyIndex = barrier.bufferInfo.dstQueueFamilyIndex;
            memoryBarrier.buffer = buffer->GetHandle();
            memoryBarrier.offset = 0;
            memoryBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(
                vkCmdBuf,
                MapPipelineStage(barrier.srcStageMask),
                MapPipelineStage(barrier.dstStageMask),
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                VK_NULL_HANDLE,
                1,
                &memoryBarrier,
                0,
                VK_NULL_HANDLE
            );
        }
        else if (barrier.image != nullptr)
        {
            auto image = static_cast<VKImage*>(barrier.image.Get());
            VkImageSubresourceRange range;
            const ImageSubresourceRange& r = barrier.imageInfo.subresourceRange;

            range.aspectMask = MapImageAspect(r.aspectMask);
            range.baseMipLevel = r.baseMipLevel;
            range.levelCount = r.levelCount;
            range.baseArrayLayer = r.baseArrayLayer;
            range.layerCount = r.layerCount;

            VkImageMemoryBarrier vkBarrier{};
            vkBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vkBarrier.pNext = VK_NULL_HANDLE;
            vkBarrier.srcAccessMask = MapAccessMask(barrier.srcAccessMask);
            vkBarrier.dstAccessMask = MapAccessMask(barrier.dstAccessMask);
            vkBarrier.oldLayout = MapImageLayout(barrier.imageInfo.oldLayout);
            vkBarrier.newLayout = MapImageLayout(barrier.imageInfo.newLayout);
            vkBarrier.srcQueueFamilyIndex = barrier.imageInfo.srcQueueFamilyIndex;
            vkBarrier.dstQueueFamilyIndex = barrier.imageInfo.dstQueueFamilyIndex;
            vkBarrier.image = image->GetImage();
            vkBarrier.subresourceRange = range;

            vkCmdPipelineBarrier(
                vkCmdBuf,
                MapPipelineStage(barrier.srcStageMask),
                MapPipelineStage(barrier.dstStageMask),
                VK_DEPENDENCY_BY_REGION_BIT,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE,
                1,
                &vkBarrier
            );

            image->NotifyLayoutChange(vkBarrier.newLayout);
        }
        else
        {
            VkMemoryBarrier memBarrier;
            memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memBarrier.pNext = VK_NULL_HANDLE;
            memBarrier.srcAccessMask = MapAccessMask(barrier.srcAccessMask);
            memBarrier.dstAccessMask = MapAccessMask(barrier.dstAccessMask);
            memoryMemoryBarriers.push_back(memBarrier);
            vkCmdPipelineBarrier(
                vkCmdBuf,
                MapPipelineStage(barrier.srcStageMask),
                MapPipelineStage(barrier.dstStageMask),
                VK_DEPENDENCY_DEVICE_GROUP_BIT,
                1,
                &memBarrier,
                0,
                VK_NULL_HANDLE,
                0,
                VK_NULL_HANDLE
            );
        }
    }
}

void VKCommandBuffer::CopyImageToBuffer(
    RefPtr<Gfx::Image> src, RefPtr<Gfx::Buffer> dst, std::span<BufferImageCopyRegion> regions
)
{
    std::vector<VkBufferImageCopy> vkRegions;

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

        vkRegions.push_back(region);
    }

    VKImage* srcImage = static_cast<VKImage*>(src.Get());
    VKBuffer* dstBuffer = static_cast<VKBuffer*>(dst.Get());

    vkCmdCopyImageToBuffer(
        vkCmdBuf,
        srcImage->GetImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstBuffer->GetHandle(),
        vkRegions.size(),
        vkRegions.data()
    );
}
} // namespace Engine::Gfx
