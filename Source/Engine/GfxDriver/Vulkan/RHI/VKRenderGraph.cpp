#include "VKRenderGraph.hpp"
#include "../VKBuffer.hpp"
#include "../VKExtensionFunc.hpp"
#include "../VKShaderProgram.hpp"
#include "../VKUtils.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"

namespace Gfx::VK::RenderGraph
{

void Graph::TrackResource(
    ResourceType type,
    void* writableResource,
    Gfx::ImageSubresourceRange range,
    VkImageLayout layout,
    VkPipelineStageFlags stages,
    VkAccessFlags access
)
{
    auto iter = resourceUsageTracks.find(writableResource);

    if (iter != resourceUsageTracks.end())
    {
        iter->second.usages.push_back({stages, access, range, layout});
    }
    else
    {
        ResourceUsageTrack track;
        track.type = type;
        track.res = writableResource;
        track.usages.push_back({stages, access, range, layout});

        resourceUsageTracks[writableResource] = track;
    }
}
void Graph::AddBarrierToRenderPass(int& visitIndex)
{
    assert(activeSchedulingCmds[visitIndex].type == VKCmdType::BeginRenderPass);
    auto& cmd = activeSchedulingCmds[visitIndex].beginRenderPass;

    // 18/01/2024: I haven't actually use subpass now, so I treat the first subpass as a combination of SetAttachment
    // and AddSubpass(0)
    if (!cmd.renderPass->GetSubpesses().empty())
    {
        auto& s = cmd.renderPass->GetSubpesses()[0];
        for (auto& c : s.colors)
        {
            Image* image = &c.imageView->GetImage();
            VKImageView* imageView = static_cast<VKImageView*>(c.imageView);

            VkAccessFlags flags = 0;
            if (c.loadOp == AttachmentLoadOperation::Load)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            if (c.storeOp == AttachmentStoreOperation::Store)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            TrackResource(
                ResourceType::Image,
                image,
                imageView ? imageView->GetSubresourceRange() : image->GetSubresourceRange(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                flags
            );
        }

        if (s.depth.has_value())
        {
            Image* image = &s.depth->imageView->GetImage();
            VKImageView* imageView = static_cast<VKImageView*>(s.depth->imageView);

            VkAccessFlags flags = 0;
            if (s.depth->loadOp == AttachmentLoadOperation::Load)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            if (s.depth->storeOp == AttachmentStoreOperation::Store)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            TrackResource(
                ResourceType::Image,
                image,
                imageView ? imageView->GetSubresourceRange() : image->GetSubresourceRange(),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                flags
            );
        }
    }

    int barrierOffset = barriers.size();
    int barrierCount = 0;
    // proceed to End Render Pass, make necessary barriers
    for (;;)
    {
        visitIndex += 1;
        auto& cmd = activeSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;
        else if (cmd.type == VKCmdType::BindResource)
        {
            recordState.bindSetCmdIndex[cmd.bindResource.set] = visitIndex;
            recordState.bindedSetUpdateNeeded[cmd.bindResource.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            recordState.bindProgramIndex = visitIndex;
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (recordState.bindedSetUpdateNeeded[i] == true)
                {
                    auto bindSetCmdIndex = recordState.bindSetCmdIndex[i];
                    auto bindProgramIndex = recordState.bindProgramIndex;
                    auto program = activeSchedulingCmds[bindProgramIndex].bindShaderProgram.program;
                    auto& bindSetCmd = activeSchedulingCmds[bindSetCmdIndex].bindResource;
                    auto resource = bindSetCmd.resource;
                    auto& writableResources = resource->GetWritableResources(bindSetCmd.set, program);
                    for (auto& w : writableResources)
                    {
                        ResourceType type;
                        switch (w.type)
                        {
                            case VKWritableGPUResource::Type::Image: type = ResourceType::Image; break;
                            case VKWritableGPUResource::Type::Buffer: type = ResourceType::Buffer; break;
                        }
                        if (type == ResourceType::Image)
                        {
                            TrackResource(
                                type,
                                w.data,
                                w.imageView ? w.imageView->GetSubresourceRange()
                                            : static_cast<VKImage*>(w.data)->GetSubresourceRange(),
                                w.layout,
                                w.stages,
                                w.access
                            );
                        }
                        else
                        {
                            TrackResource(type, w.data, Gfx::ImageSubresourceRange(), w.layout, w.stages, w.access);
                        }

                        barrierCount += MakeBarrier(w.data, resourceUsageTracks.size() - 1);
                    }
                }
            }
        }
        else if (visitIndex >= activeSchedulingCmds.size())
            break;
    }

    cmd.barrierCount = barrierCount;
    cmd.barrierOffset = barrierOffset;
}

int Graph::MakeBarrier(void* res, int usageIndex)
{
    auto iter = resourceUsageTracks.find(res);
    assert(iter != resourceUsageTracks.end());

    int barrierCount = 0;
    if (iter->second.type == ResourceType::Image)
    {
        VKImage* image = (VKImage*)iter->second.res;

        // TODO: optimize heap allocation
        std::vector<Gfx::ImageSubresourceRange> remainingRange{iter->second.usages[usageIndex].range};
        std::vector<Gfx::ImageSubresourceRange> remainingRangeSwap{};
        auto& usages = iter->second.usages;
        auto& currentUsage = usages[usageIndex];

        for (int preUsed = usageIndex - 1; preUsed >= 0; preUsed--)
        {
            auto& preUsage = usages[preUsed];

            for (auto& currentRange : remainingRange)
            {
                if (currentRange.Overlaps(preUsage.range))
                {
                    Gfx::ImageSubresourceRange overlapping = currentRange.And(preUsage.range);
                    VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_NONE;
                    VkAccessFlags srcAccess = VK_ACCESS_NONE;

                    if (HasWriteAccessMask(currentUsage.access) || preUsage.layout != currentUsage.layout)
                    {
                        // write after write or write after read
                        if (HasWriteAccessMask(preUsage.access) || HasReadAccessMask(preUsage.access))
                        {
                            srcAccess |= preUsage.access;
                            srcStages |= preUsage.stages;
                        }
                    }

                    if (HasReadAccessMask(currentUsage.access))
                    {
                        // read after write
                        if (HasWriteAccessMask(preUsage.access))
                        {
                            srcAccess |= preUsage.access;
                            srcStages |= preUsage.stages;
                        }
                    }

                    if (srcStages != VK_PIPELINE_STAGE_NONE || srcAccess != VK_ACCESS_NONE ||
                        preUsage.layout != currentUsage.layout)
                    {
                        if (srcStages == VK_PIPELINE_STAGE_NONE)
                            srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                        Barrier barrier;
                        barrier.srcStageMask = srcStages;
                        barrier.dstStageMask = currentUsage.stages;
                        VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                        imageBarrier.srcAccessMask = srcAccess;
                        imageBarrier.dstAccessMask = currentUsage.access;
                        imageBarrier.oldLayout = preUsage.layout;
                        imageBarrier.newLayout = currentUsage.layout;
                        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.subresourceRange = Gfx::MapVkImageSubresourceRange(overlapping);
                        imageBarrier.image = image->GetImage();

                        barrier.barrierCount = 1;
                        barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
                        barriers.push_back(barrier);
                        barrierCount += 1;
                        imageMemoryBarriers.push_back(imageBarrier);
                    }

                    auto remainings = currentRange.Subtract(preUsage.range);
                    remainingRangeSwap.insert(remainingRangeSwap.end(), remainings.begin(), remainings.end());
                }
                else
                {
                    remainingRangeSwap.push_back(currentRange);
                }
            }

            std::swap(remainingRange, remainingRangeSwap);
            remainingRangeSwap.clear();

            // break
            if (remainingRange.empty())
                break;
        }
    }
    else if (iter->second.type == ResourceType::Buffer)
    {
        throw std::runtime_error("not implemented");
    }

    return barrierCount;
}

void Graph::FlushBindResourceTrack() {}

void Graph::Schedule(VKCommandBuffer2& cmd)
{
    int cmdIndexOffset = activeSchedulingCmds.size();
    activeSchedulingCmds.insert(activeSchedulingCmds.end(), cmd.GetCmds().begin(), cmd.GetCmds().end());

    // add barrier to BeginRenderPass
    for (int visitIndex = cmdIndexOffset; visitIndex < activeSchedulingCmds.size(); visitIndex++)
    {
        auto& cmd = activeSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::BeginRenderPass)
            AddBarrierToRenderPass(visitIndex);
    }
}

void Graph::Execute(VkCommandBuffer vkcmd)
{
    for (auto& cmd : activeSchedulingCmds)
    {
        switch (cmd.type)
        {
            case VKCmdType::DrawIndexed:
                {
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd);
                    vkCmdDrawIndexed(
                        vkcmd,
                        cmd.drawIndexed.indexCount,
                        cmd.drawIndexed.instanceCount,
                        cmd.draw.firstVertex,
                        cmd.drawIndexed.vertexOffset,
                        cmd.drawIndexed.firstInstance
                    );
                    break;
                }
            case VKCmdType::Draw:
                {
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd);
                    vkCmdDraw(
                        vkcmd,
                        cmd.draw.vertexCount,
                        cmd.draw.instanceCount,
                        cmd.draw.firstVertex,
                        cmd.draw.firstInstance
                    );
                    break;
                }
            case VKCmdType::BeginRenderPass:
                {
                    Gfx::VKRenderPass* renderPass = cmd.beginRenderPass.renderPass;
                    VkRenderPass vkRenderPass = renderPass->GetHandle();
                    exeState.renderPass = renderPass;
                    exeState.subpassIndex = 0;

                    // framebuffer has to get inside the execution function due to how
                    // RenderPass handle swapchain image as framebuffer attachment
                    VkFramebuffer vkFramebuffer = renderPass->GetFrameBuffer();

                    auto extent = renderPass->GetExtent();
                    VkRenderPassBeginInfo renderPassBeginInfo;
                    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                    renderPassBeginInfo.pNext = VK_NULL_HANDLE;
                    renderPassBeginInfo.renderPass = vkRenderPass;
                    renderPassBeginInfo.framebuffer = vkFramebuffer;
                    renderPassBeginInfo.renderArea = {{0, 0}, {extent.width, extent.height}};
                    renderPassBeginInfo.clearValueCount = cmd.beginRenderPass.clearValueCount;
                    renderPassBeginInfo.pClearValues = cmd.beginRenderPass.clearValues;

                    auto barrierOffset = cmd.beginRenderPass.barrierOffset;
                    auto barrierCount = cmd.beginRenderPass.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

                    vkCmdBeginRenderPass(vkcmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
                    break;
                }
            case VKCmdType::EndRenderPass:
                {
                    vkCmdEndRenderPass(vkcmd);
                    exeState.renderPass = nullptr;
                    break;
                }
            case VKCmdType::Blit:
                {
                    uint32_t srcMip = cmd.blit.blitOp.srcMip.value_or(0);
                    uint32_t dstMip = cmd.blit.blitOp.dstMip.value_or(0);
                    VkImageBlit blit;
                    blit.dstOffsets[0] = {0, 0, 0};
                    blit.dstOffsets[1] = {
                        (int32_t)(cmd.blit.to->GetDescription().width / glm::pow(2, dstMip)),
                        (int32_t)(cmd.blit.to->GetDescription().height / glm::pow(2, dstMip)),
                        1};
                    VkImageSubresourceLayers dstLayers;
                    dstLayers.aspectMask = cmd.blit.to->GetDefaultSubresourceRange().aspectMask;
                    dstLayers.baseArrayLayer = 0;
                    dstLayers.layerCount = cmd.blit.to->GetDefaultSubresourceRange().layerCount;
                    dstLayers.mipLevel = dstMip;
                    blit.dstSubresource = dstLayers;

                    blit.srcOffsets[0] = {0, 0, 0};
                    blit.srcOffsets[1] = {
                        (int32_t)(cmd.blit.from->GetDescription().width / glm::pow(2, srcMip)),
                        (int32_t)(cmd.blit.from->GetDescription().height / glm::pow(2, srcMip)),
                        1};
                    VkImageSubresourceLayers srcLayers = dstLayers;
                    srcLayers.mipLevel = cmd.blit.blitOp.srcMip.value_or(0);
                    blit.srcSubresource = srcLayers; // basically copy the resources from dst
                                                     // without much configuration

                    vkCmdBlitImage(
                        vkcmd,
                        cmd.blit.from->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        cmd.blit.to->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit,
                        VK_FILTER_NEAREST
                    );
                    break;
                }
            case VKCmdType::BindResource:
                {
                    if (cmd.bindResource.set > 4)
                        return;

                    exeState.setResources[cmd.bindResource.set].resource = (VKShaderResource*)cmd.bindResource.resource;
                    exeState.setResources[cmd.bindResource.set].needUpdate = true;
                    break;
                }
            case VKCmdType::BindVertexBuffer:
                {
                    VkBuffer vkBuffers[8];
                    uint64_t vkOffsets[8];
                    for (uint32_t i = 0; i < cmd.bindVertexBuffer.vertexBufferBindingCount; ++i)
                    {
                        VKBuffer* vkbuf = static_cast<VKBuffer*>(cmd.bindVertexBuffer.vertexBufferBindings[i].buffer);
                        vkBuffers[i] = vkbuf->GetHandle();
                        vkOffsets[i] = cmd.bindVertexBuffer.vertexBufferBindings[i].offset;
                    }

                    vkCmdBindVertexBuffers(
                        vkcmd,
                        cmd.bindVertexBuffer.firstBindingIndex,
                        cmd.bindVertexBuffer.vertexBufferBindingCount,
                        vkBuffers,
                        vkOffsets
                    );
                    break;
                }
            case VKCmdType::BindShaderProgram:
                {
                    exeState.lastBindedShader = cmd.bindShaderProgram.program;
                    exeState.shaderConfig = cmd.bindShaderProgram.config;
                    break;
                }
            case VKCmdType::BindIndexBuffer:
                {
                    VKBuffer* buffer = cmd.bindIndexBuffer.buffer;

                    VkBuffer indexBuf = buffer->GetHandle();
                    vkCmdBindIndexBuffer(vkcmd, indexBuf, cmd.bindIndexBuffer.offset, cmd.bindIndexBuffer.indexType);
                    break;
                }
            case VKCmdType::SetViewport:
                {
                    vkCmdSetViewport(vkcmd, 0, 1, &cmd.setViewport.viewport);
                    break;
                }
            case VKCmdType::CopyImageToBuffer:
                {
                    std::vector<VkBufferImageCopy> vkRegions;

                    for (auto& r : cmd.copyImageToBuffer.regions)
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

                    VKImage* srcImage = cmd.copyImageToBuffer.src;
                    VKBuffer* dstBuffer = cmd.copyImageToBuffer.dst;

                    vkCmdCopyImageToBuffer(
                        vkcmd,
                        srcImage->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        dstBuffer->GetHandle(),
                        vkRegions.size(),
                        vkRegions.data()
                    );
                    break;
                }
            case VKCmdType::SetPushConstant:
                {
                    VKShaderProgram* shaderProgram = cmd.setPushConstant.shaderProgram;

                    vkCmdPushConstants(
                        vkcmd,
                        shaderProgram->GetVKPipelineLayout(),
                        cmd.setPushConstant.stages,
                        0,
                        cmd.setPushConstant.dataSize,
                        cmd.setPushConstant.data
                    );
                    break;
                }
            case VKCmdType::SetScissor:
                {
                    vkCmdSetScissor(
                        vkcmd,
                        cmd.setScissor.firstScissor,
                        cmd.setScissor.scissorCount,
                        cmd.setScissor.rects
                    );
                    break;
                }
            case VKCmdType::Dispatch:
                {
                    UpdateDescriptorSetBinding(vkcmd);
                    vkCmdDispatch(vkcmd, cmd.dispatch.groupCountX, cmd.dispatch.groupCountY, cmd.dispatch.groupCountZ);
                    break;
                }
            case VKCmdType::DispatchIndir:
                {
                    UpdateDescriptorSetBinding(vkcmd);
                    vkCmdDispatchIndirect(vkcmd, cmd.dispatchIndir.buffer->GetHandle(), cmd.dispatchIndir.bufferOffset);
                    break;
                }
            case VKCmdType::NextRenderPass:
                {
                    exeState.subpassIndex += 1;
                    vkCmdNextSubpass(vkcmd, VK_SUBPASS_CONTENTS_INLINE);
                    break;
                }
            case VKCmdType::PushDescriptorSet:
                {
                    VkWriteDescriptorSet writeSets[8];
                    VkDescriptorImageInfo imageInfos[16];

                    int i = 0;
                    int imageIndex = 0;
                    for (int bindingIndex = 0; bindingIndex < cmd.pushDescriptor.bindingCount; ++bindingIndex)
                    {
                        auto& b = cmd.pushDescriptor.bindings[bindingIndex];
                        if (b.imageView != nullptr)
                        {
                            for (int imageJedex = 0; imageJedex < b.descriptorCount; imageJedex += 1, imageIndex += 1)
                            {
                                VKImageView* vkImageView = static_cast<VKImageView*>(b.imageView + imageJedex);
                                auto layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                auto imageView = vkImageView->GetHandle();
                                imageInfos[imageIndex] = {.imageView = imageView, .imageLayout = layout};
                            }

                            writeSets[i] = {
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .pNext = VK_NULL_HANDLE,
                                .dstBinding = b.dstBinding,
                                .dstArrayElement = b.dstArrayElement,
                                .descriptorCount = b.descriptorCount,
                                .descriptorType =
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // I think should be queried from shader,
                                                                               // but shader currently don't support an
                                                                               // easy way to get the binding type (need
                                                                               // iterate through ShaderInfo.bindings)
                                .pImageInfo = imageInfos,
                            };
                        }

                        i += 1;
                    }

                    VKExtensionFunc::vkCmdPushDescriptorSetKHR(
                        vkcmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        cmd.pushDescriptor.shader->GetVKPipelineLayout(),
                        cmd.pushDescriptor.set,
                        cmd.pushDescriptor.bindingCount,
                        writeSets
                    );

                    break;
                }
            case VKCmdType::CopyBuffer:
                {
                    vkCmdCopyBuffer(
                        vkcmd,
                        cmd.copyBuffer.src->GetHandle(),
                        cmd.copyBuffer.dst->GetHandle(),
                        cmd.copyBuffer.copyRegionCount,
                        cmd.copyBuffer.copyRegions
                    );
                    break;
                }
            case VKCmdType::CopyBufferToImage:
                {
                    vkCmdCopyBufferToImage(
                        vkcmd,
                        cmd.copyBufferToImage.src->GetHandle(),
                        cmd.copyBufferToImage.dst->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        cmd.copyBufferToImage.regionCount,
                        cmd.copyBufferToImage.regions
                    );
                    break;
                }
        }
    }

    activeSchedulingCmds.clear();
    imageMemoryBarriers.clear();
    bufferMemoryBarriers.clear();
    memoryBarriers.clear();
    resourceUsageTracks.clear();
    exeState = ExecutionState();
    recordState = FlushBindResource();
}

void Graph::UpdateDescriptorSetBinding(VkCommandBuffer cmd)
{
    UpdateDescriptorSetBinding(cmd, 0);
    UpdateDescriptorSetBinding(cmd, 1);
    UpdateDescriptorSetBinding(cmd, 2);
    UpdateDescriptorSetBinding(cmd, 3);
}

void Graph::TryBindShader(VkCommandBuffer cmd)
{
    if (exeState.bindedShader != exeState.lastBindedShader && exeState.lastBindedShader != nullptr)
    {
        // binding pipeline
        auto pipeline = exeState.lastBindedShader->RequestPipeline(
            *exeState.shaderConfig,
            exeState.renderPass->GetHandle(),
            exeState.subpassIndex
        );

        if (exeState.lastBindedShader->IsCompute())
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        }
        else
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        exeState.bindedShader = exeState.lastBindedShader;
    }
}

void Graph::UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index)
{
    if (exeState.setResources[index].needUpdate && exeState.setResources[index].resource)
    {
        auto sourceSet = exeState.setResources[index].resource->GetDescriptorSet(index, exeState.lastBindedShader);
        if (sourceSet != VK_NULL_HANDLE && sourceSet != exeState.bindedDescriptorSets[index])
        {
            exeState.bindedDescriptorSets[index] = sourceSet;
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                exeState.lastBindedShader->GetVKPipelineLayout(),
                index,
                1,
                &exeState.bindedDescriptorSets[index],
                0,
                VK_NULL_HANDLE
            );
            exeState.setResources[index].needUpdate = false;
        }
    }
}

void Graph::PutBarrier(VkCommandBuffer vkcmd, int index)
{
    auto& barrier = barriers[index];
    if (barrier.bufferMemoryBarrierIndex != -1)
    {
        vkCmdPipelineBarrier(
            vkcmd,
            barrier.srcStageMask,
            barrier.dstStageMask,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            VK_NULL_HANDLE,
            barrier.barrierCount,
            &bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex],
            0,
            VK_NULL_HANDLE
        );
    }
    else if (barrier.imageMemorybarrierIndex != -1)
    {
        vkCmdPipelineBarrier(
            vkcmd,
            barrier.srcStageMask,
            barrier.dstStageMask,
            VK_DEPENDENCY_BY_REGION_BIT,
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE,
            barrier.barrierCount,
            &imageMemoryBarriers[barrier.imageMemorybarrierIndex]
        );
    }
    else if (barrier.memoryBarrierIndex != -1)
    {
        vkCmdPipelineBarrier(
            vkcmd,
            barrier.srcStageMask,
            barrier.dstStageMask,
            VK_DEPENDENCY_BY_REGION_BIT,
            barrier.barrierCount,
            &memoryBarriers[barrier.memoryBarrierIndex],
            0,
            VK_NULL_HANDLE,
            0,
            VK_NULL_HANDLE
        );
    }
}
} // namespace Gfx::VK::RenderGraph
