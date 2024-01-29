#include "VKRenderGraph.hpp"
#include "../VKBuffer.hpp"
#include "../VKDriver.hpp"
#include "../VKExtensionFunc.hpp"
#include "../VKShaderProgram.hpp"
#include "../VKShaderResource.hpp"
#include "../VKUtils.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"

namespace Gfx::VK::RenderGraph
{

bool Graph::TrackResource(
    VKImage* writableResource,
    Gfx::ImageSubresourceRange range,
    VkImageLayout layout,
    VkPipelineStageFlags stages,
    VkAccessFlags access
)
{
    auto iter = resourceUsageTracks.find(writableResource);

    if (iter != resourceUsageTracks.end())
    {
        ResourceUsage usage{stages, access, range, layout};
        if (iter->second.currentFrameUsages.empty())
        {
            iter->second.currentFrameUsages.push_back(usage);
            return true;
        }
        else
        {
            auto& lastUsage = iter->second.currentFrameUsages.back();
            if (usage != lastUsage)
            {
                iter->second.currentFrameUsages.push_back(usage);
                return true;
            }
        }
    }
    else
    {
        ResourceUsageTrack track;
        track.type = ResourceType::Image;
        track.res = writableResource;
        track.currentFrameUsages.push_back({stages, access, range, layout});

        resourceUsageTracks[writableResource] = track;
        return true;
    }

    return false;
}

bool Graph::TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access)
{
    auto iter = resourceUsageTracks.find(writableResource);

    if (iter != resourceUsageTracks.end())
    {
        ResourceUsage usage{stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED};
        if (iter->second.currentFrameUsages.empty())
        {
            iter->second.currentFrameUsages.push_back(
                {stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED}
            );

            return true;
        }
        else
        {
            auto& lastUsage = iter->second.currentFrameUsages.back();

            if (usage != lastUsage)
            {
                iter->second.currentFrameUsages.push_back(
                    {stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED}
                );

                return true;
            }
        }
    }
    else
    {
        ResourceUsageTrack track;
        track.type = ResourceType::Buffer;
        track.res = writableResource;
        track.currentFrameUsages.push_back({stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED});

        resourceUsageTracks[writableResource] = track;
        return true;
    }

    return false;
}

void Graph::AddBarrierToRenderPass(int& visitIndex)
{
    assert(currentSchedulingCmds[visitIndex].type == VKCmdType::BeginRenderPass);
    auto& cmd = currentSchedulingCmds[visitIndex].beginRenderPass;

    int barrierOffset = barriers.size();
    int barrierCount = 0;

    // handle case like shadow map being binded to global descriptor set but also set to render pass attachment
    std::vector<VKImage*> shaderImageSampleIgnoreList;
    shaderImageSampleIgnoreList.reserve(8);

    // 18/01/2024: I haven't actually use subpass now, so I treat the first subpass as a combination of SetAttachment
    // and AddSubpass(0)
    if (!cmd.renderPass->GetSubpesses().empty())
    {
        auto& s = cmd.renderPass->GetSubpesses()[0];
        for (auto& c : s.colors)
        {
            VKImage* image = static_cast<VKImage*>(&c.imageView->GetImage());

            if (image->IsSwapchainProxy())
            {
                auto swapchainImage = static_cast<VKSwapChainImage*>(image);
                image = swapchainImage->GetImage(swapchainImage->GetActiveIndex());
            }

            VKImageView* imageView = static_cast<VKImageView*>(c.imageView);

            VkAccessFlags flags = 0;
            if (c.loadOp == AttachmentLoadOperation::Load)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            if (c.storeOp == AttachmentStoreOperation::Store)
                flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            if (TrackResource(
                    static_cast<VKImage*>(image),
                    imageView ? imageView->GetSubresourceRange() : image->GetSubresourceRange(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    flags
                ))
            {
                barrierCount += MakeBarrierForLastUsage(image);
            }

            shaderImageSampleIgnoreList.push_back(image);
        }

        if (s.depth.has_value())
        {
            VKImage* image = static_cast<VKImage*>(&s.depth->imageView->GetImage());
            VKImageView* imageView = static_cast<VKImageView*>(s.depth->imageView);

            VkAccessFlags flags = 0;
            if (s.depth->loadOp == AttachmentLoadOperation::Load)
                flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            if (s.depth->storeOp == AttachmentStoreOperation::Store)
                flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            if (TrackResource(
                    static_cast<VKImage*>(image),
                    imageView ? imageView->GetSubresourceRange() : image->GetSubresourceRange(),
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                    flags
                ))
            {
                barrierCount += MakeBarrierForLastUsage(image);
            }
            shaderImageSampleIgnoreList.push_back(image);
        }
    }

    // proceed to End Render Pass, make necessary barriers
    for (;;)
    {
        visitIndex += 1;
        auto& cmd = currentSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;
        else if (cmd.type == VKCmdType::BindResource)
        {
            recordState.bindSetCmdIndex[cmd.bindResource.set] = visitIndex;
            recordState.bindedSetUpdateNeeded[cmd.bindResource.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            if (cmd.bindShaderProgram.program != recordState.bindedProgram)
            {
                recordState.bindedProgram = cmd.bindShaderProgram.program;
                recordState.bindedSetUpdateNeeded[0] = true;
                recordState.bindProgramIndex = visitIndex;
            }
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (recordState.bindedSetUpdateNeeded[i] == true)
                {
                    recordState.bindedSetUpdateNeeded[i] = false;
                    auto bindSetCmdIndex = recordState.bindSetCmdIndex[i];
                    auto bindProgramIndex = recordState.bindProgramIndex;
                    auto program = currentSchedulingCmds[bindProgramIndex].bindShaderProgram.program;
                    auto& bindSetCmd = currentSchedulingCmds[bindSetCmdIndex].bindResource;
                    uint32_t updateSet = i != 0 ? bindSetCmd.set : 0;
                    VKShaderResource* resource = i != 0 ? bindSetCmd.resource : globalResource.get();
                    auto& writableResources = resource->GetWritableResources(updateSet, program);
                    for (auto& w : writableResources)
                    {
                        ResourceType type = ResourceType::Image;
                        switch (w.type)
                        {
                            case VKWritableGPUResource::Type::Image: type = ResourceType::Image; break;
                            case VKWritableGPUResource::Type::Buffer: type = ResourceType::Buffer; break;
                        }
                        if (type == ResourceType::Image)
                        {
                            if (std::find(
                                    shaderImageSampleIgnoreList.begin(),
                                    shaderImageSampleIgnoreList.end(),
                                    w.data
                                ) != shaderImageSampleIgnoreList.end())
                            {
                                continue;
                            }

                            if (TrackResource(
                                    (VKImage*)w.data,
                                    w.imageView ? w.imageView->GetSubresourceRange()
                                                : static_cast<VKImage*>(w.data)->GetSubresourceRange(),
                                    w.layout,
                                    w.stages,
                                    w.access
                                ))
                            {
                                barrierCount += MakeBarrierForLastUsage(w.data);
                            }
                        }
                        else
                        {
                            if (TrackResource((VKBuffer*)w.data, w.stages, w.access))
                            {

                                barrierCount += MakeBarrierForLastUsage(w.data);
                            }
                        }
                    }
                }
            }
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            barrierCount += TrackResourceForPushDescriptorSet(cmd, true);
        }
        else if (visitIndex >= currentSchedulingCmds.size())
            break;
    }

    cmd.barrierCount = barrierCount;
    cmd.barrierOffset = barrierOffset;
}

int Graph::MakeBarrierForLastUsage(void* res)
{
    auto iter = resourceUsageTracks.find(res);
    assert(iter != resourceUsageTracks.end());

    int barrierCount = 0;
    auto& currentFrameUsages = iter->second.currentFrameUsages;
    auto& currentUsage = currentFrameUsages.back();
    size_t usageIndex = currentFrameUsages.size() - 1;
    size_t previousUsageIndex = 0;
    std::vector<ResourceUsage>* usagesSource = &currentFrameUsages;
    if (iter->second.type == ResourceType::Image)
    {
        VKImage* image = (VKImage*)iter->second.res;
        // TODO: optimize heap allocation
        std::vector<Gfx::ImageSubresourceRange> remainingRange{currentUsage.range};
        std::vector<Gfx::ImageSubresourceRange> remainingRangeSwap{};
        for (;;)
        {
            if (usageIndex == 0)
            {
                if (!iter->second.previousFrameUsages.empty())
                {
                    usagesSource = &iter->second.previousFrameUsages;
                    usageIndex = iter->second.previousFrameUsages.size() - 1;
                }
                else
                    break;
            }
            else
            {
                usageIndex -= 1;
            }
            auto& preUsage = (*usagesSource)[usageIndex];

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
                        {
                            srcStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                            srcAccess = VK_ACCESS_NONE;
                        }
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

        // handle the first frame when the image layout is undefined
        if (usageIndex == 0 && barrierCount == 0)
        {
            Barrier barrier;
            barrier.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            barrier.dstStageMask = currentUsage.stages;

            VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            imageBarrier.srcAccessMask = VK_ACCESS_NONE;
            imageBarrier.dstAccessMask = currentUsage.access;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier.newLayout = currentUsage.layout;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.subresourceRange = Gfx::MapVkImageSubresourceRange(currentUsage.range);
            imageBarrier.image = image->GetImage();

            barrier.barrierCount = 1;
            barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
            barriers.push_back(barrier);
            barrierCount += 1;
            imageMemoryBarriers.push_back(imageBarrier);
        }
    }
    else if (iter->second.type == ResourceType::Buffer)
    {
        VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkAccessFlags srcAccessMask = VK_ACCESS_NONE;

        if (usageIndex == 0)
        {
            if (!iter->second.previousFrameUsages.empty())
            {
                usagesSource = &iter->second.previousFrameUsages;
                previousUsageIndex = iter->second.previousFrameUsages.size() - 1;
            }
        }
        else
            previousUsageIndex = usageIndex - 1;

        if (usageIndex != 0 || usagesSource != &currentFrameUsages)
        {
            auto& preUsage = (*usagesSource)[previousUsageIndex];
            if (HasWriteAccessMask(currentUsage.access))
            {
                // write after write
                if (HasWriteAccessMask(preUsage.access))
                {
                    srcAccessMask |= preUsage.access;
                    srcStages |= preUsage.stages;
                }

                // write after read
                if (HasReadAccessMask(preUsage.access))
                {
                    srcAccessMask |= preUsage.access;
                    srcStages |= preUsage.stages;
                }
            }

            if (HasReadAccessMask(currentUsage.access))
            {
                // read after write
                if (HasWriteAccessMask(preUsage.access))
                {
                    srcAccessMask |= preUsage.access;
                    srcStages |= preUsage.stages;
                }
            }

            if (srcStages != VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT || srcAccessMask != VK_ACCESS_NONE)
            {
                Barrier barrier;
                barrier.srcStageMask = srcStages;
                barrier.dstStageMask = currentUsage.stages;
                VkBufferMemoryBarrier bufferBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                bufferBarrier.srcAccessMask =
                    srcStages == VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT ? VK_ACCESS_NONE : srcAccessMask;
                bufferBarrier.dstAccessMask = currentUsage.access;
                bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                bufferBarrier.buffer = ((VKBuffer*)iter->second.res)->GetHandle();
                bufferBarrier.offset = 0;
                bufferBarrier.size = VK_WHOLE_SIZE;

                barrier.barrierCount = 1;
                barrier.bufferMemoryBarrierIndex = bufferMemoryBarriers.size();
                bufferMemoryBarriers.push_back(bufferBarrier);
                barriers.push_back(barrier);
                barrierCount += 1;
            }
        }
    }
    return barrierCount;
}

void Graph::FlushBindResourceTrack() {}

size_t Graph::TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier)
{
    int imageIndex = 0;
    int barrierCount = 0;
    for (int bindingIndex = 0; bindingIndex < cmd.pushDescriptor.bindingCount; ++bindingIndex)
    {
        auto& b = cmd.pushDescriptor.bindings[bindingIndex];
        if (b.imageView != nullptr)
        {
            for (int imageJedex = 0; imageJedex < b.descriptorCount; imageJedex += 1, imageIndex += 1)
            {
                VKImageView* vkImageView = static_cast<VKImageView*>(b.imageView + imageJedex);
                auto layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                auto image = static_cast<VKImage*>(&vkImageView->GetImage());
                if (image->IsGPUWrite())
                {
                    if (TrackResource(
                            image,
                            vkImageView->GetSubresourceRange(),
                            layout,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                            VK_ACCESS_2_MEMORY_READ_BIT
                        ) &&
                        addBarrier)
                    {
                        barrierCount += MakeBarrierForLastUsage(image);
                    }
                }
            }
        }
    }

    return barrierCount;
}
void Graph::Schedule(VKCommandBuffer2& cmd)
{
    int cmdIndexOffset = currentSchedulingCmds.size();
    currentSchedulingCmds.insert(currentSchedulingCmds.end(), cmd.GetCmds().begin(), cmd.GetCmds().end());

    // track where to put barriers
    for (int visitIndex = cmdIndexOffset; visitIndex < currentSchedulingCmds.size(); visitIndex++)
    {
        auto& cmd = currentSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::BeginRenderPass)
            AddBarrierToRenderPass(visitIndex);
        else if (cmd.type == VKCmdType::BindResource)
        {
            recordState.bindSetCmdIndex[cmd.bindResource.set] = visitIndex;
            recordState.bindedSetUpdateNeeded[cmd.bindResource.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            if (cmd.bindShaderProgram.program != recordState.bindedProgram)
            {
                recordState.bindedProgram = cmd.bindShaderProgram.program;
                recordState.bindedSetUpdateNeeded[0] = true;
                recordState.bindProgramIndex = visitIndex;
            }
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            if (TrackResource(cmd.copyBuffer.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                barrierCount += MakeBarrierForLastUsage(cmd.copyBuffer.src);

            if (TrackResource(cmd.copyBuffer.dst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
                barrierCount += MakeBarrierForLastUsage(cmd.copyBuffer.dst);

            cmd.copyBuffer.barrierOffset = barrierOffset;
            cmd.copyBuffer.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::Blit)
        {
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            Gfx::ImageSubresourceRange srcRange{
                .aspectMask = Gfx::MapVKImageAspect(cmd.blit.to->GetDefaultSubresourceRange().aspectMask),
                .baseMipLevel = cmd.blit.blitOp.srcMip.value_or(0),
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = cmd.blit.to->GetDefaultSubresourceRange().layerCount,
            };

            if (TrackResource(
                    cmd.blit.from,
                    srcRange,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT
                ))
                barrierCount += MakeBarrierForLastUsage(cmd.blit.from);

            Gfx::ImageSubresourceRange dstRange{
                .aspectMask = Gfx::MapVKImageAspect(cmd.blit.to->GetDefaultSubresourceRange().aspectMask),
                .baseMipLevel = cmd.blit.blitOp.dstMip.value_or(0),
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = cmd.blit.to->GetDefaultSubresourceRange().layerCount,
            };

            if (TrackResource(
                    cmd.blit.from,
                    dstRange,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT
                ))
                barrierCount += MakeBarrierForLastUsage(cmd.blit.to);

            cmd.blit.barrierOffset = barrierOffset;
            cmd.blit.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::CopyBufferToImage)
        {
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            if (TrackResource(cmd.copyBufferToImage.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                barrierCount += MakeBarrierForLastUsage(cmd.copyBufferToImage.src);

            for (int i = 0; i < cmd.copyBufferToImage.regionCount; ++i)
            {
                auto& subresource = cmd.copyBufferToImage.regions[i].imageSubresource;
                Gfx::ImageSubresourceRange range{
                    .aspectMask = Gfx::MapVKImageAspect(subresource.aspectMask),
                    .baseMipLevel = subresource.mipLevel,
                    .levelCount = 1,
                    .baseArrayLayer = subresource.baseArrayLayer,
                    .layerCount = subresource.layerCount,
                };

                if (TrackResource(
                        cmd.copyBufferToImage.dst,
                        range,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT
                    ))
                    barrierCount += MakeBarrierForLastUsage(cmd.copyBufferToImage.dst);
            }

            cmd.copyBufferToImage.barrierOffset = barrierOffset;
            cmd.copyBufferToImage.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            TrackResourceForPushDescriptorSet(cmd, false);
        }
        else if (cmd.type == VKCmdType::Present)
        {
            Gfx::ImageSubresourceRange range{
                .aspectMask = ImageAspectFlags::Color,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            };
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            if (TrackResource(
                    cmd.present.image,
                    range,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_NONE
                ))
            {
                barrierCount += MakeBarrierForLastUsage(cmd.present.image);
            }
            cmd.present.barrierOffset = barrierOffset;
            cmd.present.barrierCount = barrierCount;
        }
    }
}

void Graph::Execute(VkCommandBuffer vkcmd)
{
    for (size_t i = 0; i < currentSchedulingCmds.size(); ++i)
    {
        auto& cmd = currentSchedulingCmds[i];
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
                        1
                    };
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
                        1
                    };
                    VkImageSubresourceLayers srcLayers = dstLayers;
                    srcLayers.mipLevel = cmd.blit.blitOp.srcMip.value_or(0);
                    blit.srcSubresource = srcLayers; // basically copy the resources from dst
                                                     // without much configuration

                    auto barrierOffset = cmd.blit.barrierOffset;
                    auto barrierCount = cmd.blit.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

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

                    auto barrierOffset = cmd.copyImageToBuffer.barrierOffset;
                    auto barrierCount = cmd.copyImageToBuffer.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

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
                            i += 1;
                        }
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
                    auto barrierOffset = cmd.copyBuffer.barrierOffset;
                    auto barrierCount = cmd.copyBuffer.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

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
                    auto barrierOffset = cmd.copyBufferToImage.barrierOffset;
                    auto barrierCount = cmd.copyBufferToImage.barrierCount;

                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
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
            case VKCmdType::Present:
                {
                    auto barrierOffset = cmd.present.barrierOffset;
                    auto barrierCount = cmd.present.barrierCount;

                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
                    break;
                }
            case VKCmdType::SetTexture:
                {
                    globalResource->SetImage(cmd.setTexture.handle, cmd.setTexture.image);
                    break;
                }
            case VKCmdType::SetUniformBuffer:
                {
                    globalResource->SetBuffer(cmd.setUniformBuffer.handle, cmd.setUniformBuffer.buffer);
                    break;
                }
            case VKCmdType::None: break;
        }
    }

    std::swap(currentSchedulingCmds, previousSchedulingCmds);
    currentSchedulingCmds.clear();
    for (auto& r : resourceUsageTracks)
    {
        std::swap(r.second.currentFrameUsages, r.second.previousFrameUsages);
        r.second.currentFrameUsages.clear();
    }
    exeState = ExecutionState();
    recordState = FlushBindResource();

    imageMemoryBarriers.clear();
    bufferMemoryBarriers.clear();
    memoryBarriers.clear();
    barriers.clear();
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
    Barrier& barrier = barriers[index];
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
Graph::Graph()
{
    globalResource = std::make_unique<VKShaderResource>();
}
} // namespace Gfx::VK::RenderGraph
