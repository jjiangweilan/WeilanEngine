#include "VKCommandBufferProcessor.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKRayTracingContext.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Hash.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKDriver.hpp"

#include "RayTracing/VKRayTracing.hpp"
#include "VKBuffer.hpp"
#include "VKShaderProgram.hpp"
#include "VKShaderResource.hpp"
#include "VKUtils.hpp"
#include <array>
#include <functional>

namespace Gfx
{

static void PlaceFullMemoryBarrier(VkCommandBuffer cmd)
{
    VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        1,
        &memoryBarrier,
        0,
        nullptr,
        0,
        nullptr
    );
}

static VkPipelineStageFlags ShaderStageToPipelineStage(ShaderStage stages)
{
    VkPipelineStageFlags pipelineStages = 0;
    if (HasFlag(stages, ShaderStage::Vertex))
        pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    if (HasFlag(stages, ShaderStage::Fragment))
        pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    if (HasFlag(stages, ShaderStage::Compute))
        pipelineStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    return pipelineStages;
}
void VKCommandBufferProcessor::RemoveImageRelatedInfo(Image* ptr)
{
    for (auto& r : globalResourcePool)
    {
        for (auto& e : r.second)
        {
            if (e.second.type == ResourceType::Image && std::get<ObjPtr<Image>>(e.second.res).Get() == ptr)
            {
                e.second.res = ObjPtr<Image>(nullptr);
            }
        }
    }

    resourceUsageTracks.erase(ptr->GetUUID());
}

bool VKCommandBufferProcessor::TrackResource(
    VKImage* writableResource,
    Gfx::ImageSubresourceRange range,
    VkImageLayout layout,
    VkPipelineStageFlags stages,
    VkAccessFlags access
)
{
    auto iter = resourceUsageTracks.find(writableResource->GetUUID());

    if (iter != resourceUsageTracks.end())
    {
        // because we don't enale separate depth/stencil, when range comes from a depth only imageView, we need to track
        // both depth and stencil for aspectMask force a stencil flag when the original format has stencil
        if (Gfx::HasStencil(writableResource->GetDescription().format))
        {
            range.aspectMask |= Gfx::ImageAspect::Stencil;
        }

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
        track.res = ObjPtr<Image>(writableResource);
        track.currentFrameUsages.push_back({stages, access, range, layout});

        resourceUsageTracks[writableResource->GetUUID()] = track;
        return true;
    }

    return false;
}

VKImage* VKCommandBufferProcessor::Request(const ImageIdentifier& id, RenderImageDescriptor& desc)
{
    return VKContext::Instance()->resourceAllocator->Request(id, desc);
}

VKRenderPass* VKCommandBufferProcessor::Request(RenderPass& renderPass)
{
    return VKContext::Instance()->resourceAllocator->Request(renderPass);
}

bool VKCommandBufferProcessor::TrackResource(
    VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access
)
{
    ENGINE_SCOPED_PROFILE("TrackResource");
    auto iter = resourceUsageTracks.find(writableResource->GetUUID());

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
        track.res = ObjPtr<Buffer>(writableResource);
        track.currentFrameUsages.push_back({stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED});

        resourceUsageTracks[writableResource->GetUUID()] = track;
        return true;
    }

    return false;
}

bool VKCommandBufferProcessor::TrackResource(const VKBufferResourceRef& ref, VkPipelineStageFlags stages, VkAccessFlags access)
{
    if (ref.buffer == VK_NULL_HANDLE || ref.trackingId == 0)
        return false;

    ENGINE_SCOPED_PROFILE("TrackResource");

    ResourceUsage usage{stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED};

    if (ref.rawBuffer != nullptr)
    {
        return TrackResource(ref.rawBuffer, stages, access);
    }

    auto iter = tempResourceUsageTracks.find(ref.trackingId);
    if (iter != tempResourceUsageTracks.end())
    {
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
        auto& track = tempResourceUsageTracks[ref.trackingId];
        track.type = ResourceType::Buffer;
        track.res = ObjPtr<Buffer>(nullptr);
        track.currentFrameUsages.push_back(
            {stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED}
        );
        return true;
    }

    return false;
}

void VKCommandBufferProcessor::GoThroughRenderPass(
    int inflightIndex,
    std::vector<VKCmd>& exectedCmds,
    VKRenderPass& renderPass,
    int& visitIndex,
    int& barrierCountResult,
    int& barrierOffsetResult,
    uint32_t& hoistedTransferOffsetResult,
    uint32_t& hoistedTransferCountResult
)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - GoThroughRenderPass");
    uint32_t hoistedTransferOffset = static_cast<uint32_t>(hoistedTransferCmdIndices.size());

    for (int scanIndex = visitIndex + 1; scanIndex < exectedCmds.size(); ++scanIndex)
    {
        VKCmd& cmd = exectedCmds[scanIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;

        if (cmd.type == VKCmdType::AllocateBuffer)
        {
            AllocateTemporaryBufferCmd(std::get<VKAllocateBufferCmd>(cmd.args), inflightIndex);
        }
        else if (cmd.type == VKCmdType::UploadData)
        {
            PrepareUploadDataCmd(std::get<VKUploadDataCmd>(cmd.args), inflightIndex);
            hoistedTransferCmdIndices.push_back(static_cast<uint32_t>(scanIndex));
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {
            PrepareCopyBufferCmd(std::get<VKCopyBufferCmd>(cmd.args), inflightIndex);
            hoistedTransferCmdIndices.push_back(static_cast<uint32_t>(scanIndex));
        }
        else if (IsUnsupportedRenderPassTransferCmd(cmd.type))
        {
            SPDLOG_ERROR("VKCommandBufferProcessor: unsupported transfer command recorded inside render pass");
        }
    }

    int barrierOffset = barriers.size();
    int barrierCount = 0;

    // handle case like shadow map being binded to global descriptor set but also set to render pass attachment
    std::vector<VKImage*> shaderImageSampleIgnoreList;
    shaderImageSampleIgnoreList.reserve(8);
    std::array<VKBuffer*, 8> boundVertexBuffers{};
    uint32_t boundVertexBufferCount = 0;
    VKBuffer* boundIndexBuffer = nullptr;

    // 18/01/2024: I haven't actually use subpass now, so I treat the first subpass as a combination of SetAttachment
    // and AddSubpass(0)
    if (!renderPass.GetSubpesses().empty())
    {
        auto& s = renderPass.GetSubpesses()[0];
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
                barrierCount += MakeBarrierForLastUsage2(image);
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
                barrierCount += MakeBarrierForLastUsage2(image);
            }
            shaderImageSampleIgnoreList.push_back(image);
        }
    }

    // proceed to End Render Pass, make necessary barriers
    for (;;)
    {
        visitIndex += 1;
        auto& cmd = exectedCmds[visitIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;
        else if (cmd.type == VKCmdType::BindResource)
        {
            recordState.bindSetCmdIndex[std::get<VKBindResourceCmd>(cmd.args).set] = visitIndex;
            recordState.bindedSetUpdateNeeded[std::get<VKBindResourceCmd>(cmd.args).set] = true;
        }
        else if (cmd.type == VKCmdType::AllocateBuffer || IsHoistedRenderPassTransferCmd(cmd.type) || IsUnsupportedRenderPassTransferCmd(cmd.type))
        {}
        else if (cmd.type == VKCmdType::DynamicBindResource)
        {
            auto& args = std::get<VKDynamicBindResourceCmd>(cmd.args);
            recordState.dynamicBindSetCmdIndex[args.set] = visitIndex;
            recordState.dynamicBindedSetUpdateNeeded[args.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            ScheduleBindShaderProgram(cmd, visitIndex);
        }
        else if (cmd.type == VKCmdType::BindVertexBuffer)
        {
            auto& args = std::get<VKBindVertexBufferCmd>(cmd.args);
            boundVertexBufferCount = args.vertexBufferBindingCount;
            for (uint32_t i = 0; i < boundVertexBufferCount && i < boundVertexBuffers.size(); ++i)
            {
                if (args.vertexBufferBindings[i].buffer.type == BufferIdentifier::Type::RawBuffer)
                    boundVertexBuffers[i] = static_cast<VKBuffer*>(args.vertexBufferBindings[i].buffer.buffer);
                else
                    boundVertexBuffers[i] = nullptr;
            }
        }
        else if (cmd.type == VKCmdType::BindIndexBuffer)
        {
            auto& args = std::get<VKBindIndexBufferCmd>(cmd.args);
            if (args.buffer.type == BufferIdentifier::Type::RawBuffer)
                boundIndexBuffer = static_cast<VKBuffer*>(args.buffer.buffer);
            else
                boundIndexBuffer = nullptr;
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed || cmd.type == VKCmdType::DrawIndirect || cmd.type == VKCmdType::DrawIndexedIndirect)
        {
            for (uint32_t i = 0; i < boundVertexBufferCount && i < boundVertexBuffers.size(); ++i)
            {
                auto buffer = boundVertexBuffers[i];
                if (buffer && TrackResource(buffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT))
                    barrierCount += MakeBarrierForLastUsage(buffer, buffer->GetUUID());
            }

            if ((cmd.type == VKCmdType::DrawIndexed || cmd.type == VKCmdType::DrawIndexedIndirect) && boundIndexBuffer &&
                TrackResource(boundIndexBuffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT))
            {
                barrierCount += MakeBarrierForLastUsage(boundIndexBuffer, boundIndexBuffer->GetUUID());
            }

            if (cmd.type == VKCmdType::DrawIndirect)
            {
                auto& args = std::get<VKDrawIndirectCmd>(cmd.args);
                if (args.buffer.type == BufferIdentifier::Type::RawBuffer)
                {
                    auto refBuf = ResolveTrackableBuffer(args.buffer, inflightIndex);
                    if (TrackResource(refBuf, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                        barrierCount += MakeBarrierForLastUsage(refBuf);
                }
            }
            else if (cmd.type == VKCmdType::DrawIndexedIndirect)
            {
                auto& args = std::get<VKDrawIndexedIndirectCmd>(cmd.args);
                if (args.buffer.type == BufferIdentifier::Type::RawBuffer)
                {
                    auto refBuf = ResolveTrackableBuffer(args.buffer, inflightIndex);
                    if (TrackResource(refBuf, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                        barrierCount += MakeBarrierForLastUsage(refBuf);
                }
            }

            MakeBarrierForAllDynamicBindedSetUpdate(exectedCmds, shaderImageSampleIgnoreList, barrierCount);
            FlushAllBindedSetUpdate(inflightIndex, exectedCmds, shaderImageSampleIgnoreList, barrierCount);
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            barrierCount += TrackResourceForPushDescriptorSet(cmd, true);
        }
        else if (cmd.type == VKCmdType::SetTexture)
        {
            auto& args = std::get<VKSetTextureCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] = {
                ResourceType::Image,
                args.image != nullptr ? ObjPtr<Image>(args.image) : nullptr,
                args.imageViewOption
            };
        }
        else if (cmd.type == VKCmdType::SetBuffer)
        {
            auto& args = std::get<VKSetBufferCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {ResourceType::Buffer, args.buffer, std::nullopt};
        }
        else if (visitIndex >= exectedCmds.size())
            break;
    }

    barrierCountResult = barrierCount;
    barrierOffsetResult = barrierOffset;
    hoistedTransferOffsetResult = hoistedTransferOffset;
    hoistedTransferCountResult = static_cast<uint32_t>(hoistedTransferCmdIndices.size()) - hoistedTransferOffset;
}

int VKCommandBufferProcessor::MakeBarrierForLastUsage(void* res, const UUID& uuid)
{
    auto iter = resourceUsageTracks.find(uuid);
    ASSERT(iter != resourceUsageTracks.end());

    int barrierCount = 0;
    auto& currentFrameUsages = iter->second.currentFrameUsages;
    auto& currentUsage = currentFrameUsages.back();
    size_t usageIndex = currentFrameUsages.size() - 1;
    size_t previousUsageIndex = 0;
    std::vector<ResourceUsage>* usagesSource = &currentFrameUsages;
    if (iter->second.type == ResourceType::Image)
    {
        VKImage* image = (VKImage*)std::get<ObjPtr<Image>>(iter->second.res).Get();
        if (image == nullptr)
        {
            // garbage image remove it
            resourceUsageTracks.erase(iter);
            return 0;
        }

        std::vector<Gfx::ImageSubresourceRange> remainingRange({currentUsage.range});
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

                    VkImageSubresourceRange subresourceRange = Gfx::MapVkImageSubresourceRange(overlapping);
                    preUsage.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                    if (!image->QueryLayout(subresourceRange, preUsage.layout)) [[unlikely]]
                    {
                        spdlog::error("VKCommandBufferProcessor: image layout not properly handled");
                    }
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
                        barrier.targetImage = image;
                        barrier.srcStageMask = srcStages;
                        barrier.dstStageMask = currentUsage.stages;
                        VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                        imageBarrier.srcAccessMask = srcAccess;
                        imageBarrier.dstAccessMask = currentUsage.access;
                        imageBarrier.oldLayout = preUsage.layout;
                        imageBarrier.newLayout = currentUsage.layout;
                        imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                        imageBarrier.subresourceRange = subresourceRange;
                        imageBarrier.image = image->GetImage();

                        barrier.barrierCount = 1;
                        barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
                        barriers.push_back(barrier);
                        barrierCount += 1;
                        imageMemoryBarriers.push_back(imageBarrier);
                        image->SetLayout(subresourceRange, currentUsage.layout, srcStages, imageBarrier.srcAccessMask, currentUsage.stages, imageBarrier.dstAccessMask);
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

            // cover the situation when there is no overlapping range
            // in this cast the imagelayout should be UNDEFINED
            for (int i = 0; i < remainingRange.size(); ++i)
            {
                auto& range = remainingRange[i];
                VkImageSubresourceRange vkRange = Gfx::MapVkImageSubresourceRange(range);
                if (image->IsLayout(vkRange, VK_IMAGE_LAYOUT_UNDEFINED))
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
                    imageBarrier.subresourceRange = vkRange;
                    imageBarrier.image = image->GetImage();

                    barrier.barrierCount = 1;
                    barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
                    barrier.targetImage = image;
                    barriers.push_back(barrier);
                    barrierCount += 1;
                    imageMemoryBarriers.push_back(imageBarrier);
                    image->SetLayout(vkRange, currentUsage.layout, barrier.srcStageMask, imageBarrier.srcAccessMask, barrier.dstStageMask, imageBarrier.dstAccessMask);
                    remainingRange.pop_back();
                    i -= 2;
                }
            }

            remainingRangeSwap.clear();
            // break
            if (remainingRange.empty())
                break;
        }

        // handle the situation when there is no previous usage
        auto subresourceRange = Gfx::MapVkImageSubresourceRange(currentUsage.range);
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!image->QueryLayout(subresourceRange, layout)) [[unlikely]]
        {
            spdlog::error("VKCommandBufferProcessor: image layout not properly handled");
        }
        if (usageIndex == 0 && barrierCount == 0 && layout != currentUsage.layout)
        {
            Barrier barrier;
            barrier.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            barrier.dstStageMask = currentUsage.stages;
            VkImageMemoryBarrier imageBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            imageBarrier.srcAccessMask = VK_ACCESS_NONE;
            imageBarrier.dstAccessMask = currentUsage.access;
            imageBarrier.oldLayout = layout;
            imageBarrier.newLayout = currentUsage.layout;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.subresourceRange = subresourceRange;
            imageBarrier.image = image->GetImage();

            barrier.barrierCount = 1;
            barrier.imageMemorybarrierIndex = imageMemoryBarriers.size();
            barrier.targetImage = image;
            barriers.push_back(barrier);
            barrierCount += 1;
            imageMemoryBarriers.push_back(imageBarrier);
            image->SetLayout(subresourceRange, currentUsage.layout, barrier.srcStageMask, imageBarrier.srcAccessMask, barrier.dstStageMask, imageBarrier.dstAccessMask);
        }
    }
    else if (iter->second.type == ResourceType::Buffer)
    {
        VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        VkAccessFlags srcAccessMask = VK_ACCESS_NONE;
        VKBuffer* buffer = ((VKBuffer*)std::get<ObjPtr<Buffer>>(iter->second.res).Get());
        if (buffer == nullptr)
        {
            // garbage buffer remove it
            resourceUsageTracks.erase(iter);
            return 0;
        }

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
                bufferBarrier.buffer = buffer->GetHandle();
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

int VKCommandBufferProcessor::MakeBarrierForLastUsage(const VKBufferResourceRef& ref)
{
    if (ref.buffer == VK_NULL_HANDLE || ref.trackingId == 0)
        return 0;

    if (ref.rawBuffer != nullptr)
    {
        return MakeBarrierForLastUsage(ref.rawBuffer, ref.rawBuffer->GetUUID());
    }

    auto iter = tempResourceUsageTracks.find(ref.trackingId);
    if (iter == tempResourceUsageTracks.end())
        return 0;

    auto& currentFrameUsages = iter->second.currentFrameUsages;
    if (currentFrameUsages.empty())
        return 0;

    auto& currentUsage = currentFrameUsages.back();
    int barrierCount = 0;

    size_t usageIndex = currentFrameUsages.size() - 1;
    size_t previousUsageIndex = 0;
    std::vector<ResourceUsage>* usagesSource = &currentFrameUsages;

    if (usageIndex == 0)
    {
        if (!iter->second.previousFrameUsages.empty())
        {
            usagesSource = &iter->second.previousFrameUsages;
            previousUsageIndex = iter->second.previousFrameUsages.size() - 1;
        }
        else
            return 0;
    }
    else
        previousUsageIndex = usageIndex - 1;

    VkPipelineStageFlags srcStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkAccessFlags srcAccessMask = VK_ACCESS_NONE;

    auto& preUsage = (*usagesSource)[previousUsageIndex];
    if (HasWriteAccessMask(currentUsage.access))
    {
        if (HasWriteAccessMask(preUsage.access))
        {
            srcAccessMask |= preUsage.access;
            srcStages |= preUsage.stages;
        }
        if (HasReadAccessMask(preUsage.access))
        {
            srcAccessMask |= preUsage.access;
            srcStages |= preUsage.stages;
        }
    }

    if (HasReadAccessMask(currentUsage.access))
    {
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
        bufferBarrier.srcAccessMask = srcAccessMask;
        bufferBarrier.dstAccessMask = currentUsage.access;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = ref.buffer;
        bufferBarrier.offset = ref.offset;
        bufferBarrier.size = ref.size;
        barrier.bufferMemoryBarrierIndex = bufferMemoryBarriers.size();
        barriers.push_back(barrier);
        bufferMemoryBarriers.push_back(bufferBarrier);
        barrierCount += 1;
    }

    return barrierCount;
}

int VKCommandBufferProcessor::TrackUploadDataDestination(BufferIdentifier dst, int inflightIndex)
{
    auto refDst = ResolveTrackableBuffer(dst, inflightIndex);
    if (TrackResource(refDst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
        return MakeBarrierForLastUsage(refDst);

    return 0;
}

bool VKCommandBufferProcessor::IsHoistedRenderPassTransferCmd(VKCmdType type) const
{
    return type == VKCmdType::UploadData || type == VKCmdType::CopyBuffer;
}

bool VKCommandBufferProcessor::IsUnsupportedRenderPassTransferCmd(VKCmdType type) const
{
    return type == VKCmdType::CopyBufferToImage || type == VKCmdType::CopyImageToBuffer || type == VKCmdType::Blit;
}

void VKCommandBufferProcessor::AllocateTemporaryBufferCmd(VKAllocateBufferCmd& args, int inflightIndex)
{
    auto handle = GetMemAllocator()->AllocateScratchBuffer(args.size, args.alignment, args.usage, false);
    VKResolvedTemporaryBuffer resolved{
        .buffer = handle.buffer,
        .deviceAddress = handle.deviceAddress,
        .mappedData = handle.mappedData,
        .offset = handle.offset,
        .size = handle.size,
    };
    temporaryBuffers[inflightIndex][args.handle.id] = resolved;
}

void VKCommandBufferProcessor::PrepareCopyBufferCmd(VKCopyBufferCmd& args, int inflightIndex)
{
    size_t barrierOffset = barriers.size();
    size_t barrierCount = 0;
    if (args.src.type == BufferIdentifier::Type::RawBuffer || args.src.type == BufferIdentifier::Type::TemporaryBuffer)
    {
        auto refSrc = ResolveTrackableBuffer(args.src, inflightIndex);
        if (TrackResource(refSrc, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
            barrierCount += MakeBarrierForLastUsage(refSrc);
    }

    if (args.dst.type == BufferIdentifier::Type::RawBuffer || args.dst.type == BufferIdentifier::Type::TemporaryBuffer)
    {
        auto refDst = ResolveTrackableBuffer(args.dst, inflightIndex);
        if (TrackResource(refDst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
            barrierCount += MakeBarrierForLastUsage(refDst);
    }

    args.barrierOffset = static_cast<int>(barrierOffset);
    args.barrierCount = static_cast<int>(barrierCount);
}

void VKCommandBufferProcessor::PrepareUploadDataCmd(VKUploadDataCmd& args, int inflightIndex)
{
    size_t barrierOffset = barriers.size();
    size_t barrierCount = 0;
    if (args.dst.IsBuffer())
    {
        barrierCount += TrackUploadDataDestination(args.dst, inflightIndex);
    }
    args.barrierOffset = static_cast<int>(barrierOffset);
    args.barrierCount = static_cast<int>(barrierCount);
}

void VKCommandBufferProcessor::ExecuteCopyBufferCmd(VkCommandBuffer vkcmd, VKCopyBufferCmd& args, int inflightIndex)
{
    PutBarriers(vkcmd, args.barrierOffset, args.barrierCount);

    auto resolvedSrc = ResolveBuffer(args.src, inflightIndex);
    auto resolvedDst = ResolveBuffer(args.dst, inflightIndex);
    VkBufferCopy adjustedRegions[8];
    for (uint32_t i = 0; i < args.copyRegionCount; ++i)
    {
        adjustedRegions[i] = args.copyRegions[i];
        adjustedRegions[i].srcOffset += resolvedSrc.offset;
        adjustedRegions[i].dstOffset += resolvedDst.offset;
    }

    vkCmdCopyBuffer(vkcmd, resolvedSrc.buffer, resolvedDst.buffer, args.copyRegionCount, adjustedRegions);
}

void VKCommandBufferProcessor::ExecuteUploadDataCmd(VkCommandBuffer vkcmd, VKUploadDataCmd& args, int inflightIndex)
{
    PutBarriers(vkcmd, args.barrierOffset, args.barrierCount);

    auto stagingHandle = GetMemAllocator()->AllocateScratchBuffer(
        static_cast<uint32_t>(args.data.size()),
        4,
        TemporaryBufferUsage::TransferSrc,
        true
    );
    memcpy(stagingHandle.mappedData, args.data.data(), args.data.size());

    auto resolvedDst = ResolveBuffer(args.dst, inflightIndex);
    VkDeviceSize srcOffset = stagingHandle.offset;
    VkBufferCopy region{.srcOffset = srcOffset, .dstOffset = args.dstOffset + resolvedDst.offset, .size = args.data.size()};
    vkCmdCopyBuffer(vkcmd, stagingHandle.buffer, resolvedDst.buffer, 1, &region);
}

void VKCommandBufferProcessor::ExecuteHoistedRenderPassTransfers(
    VkCommandBuffer vkcmd,
    std::vector<VKCmd>& executedCmds,
    uint32_t hoistedTransferOffset,
    uint32_t hoistedTransferCount,
    int inflightIndex
)
{
    for (uint32_t i = 0; i < hoistedTransferCount; ++i)
    {
        VKCmd& cmd = executedCmds[hoistedTransferCmdIndices[hoistedTransferOffset + i]];
        if (cmd.type == VKCmdType::UploadData)
        {
            ExecuteUploadDataCmd(vkcmd, std::get<VKUploadDataCmd>(cmd.args), inflightIndex);
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {
            ExecuteCopyBufferCmd(vkcmd, std::get<VKCopyBufferCmd>(cmd.args), inflightIndex);
        }
    }
}

void VKCommandBufferProcessor::FlushBindResourceTrack() {}

size_t VKCommandBufferProcessor::TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - TrackResourceForPushDescriptorSet");
    int imageCount = 0;
    int barrierCount = 0;
    auto& pushDescriptorCmd = std::get<VKPushDescriptorCmd>(cmd.args);
    for (int bindingIndex = 0; bindingIndex < pushDescriptorCmd.bindingCount; ++bindingIndex)
    {
        auto& b = pushDescriptorCmd.bindings[bindingIndex];
        if (b.imageView != nullptr)
        {
            for (int imageIndex = 0; imageIndex < b.descriptorCount; imageIndex += 1, imageCount += 1)
            {
                VKImageView* vkImageView = static_cast<VKImageView*>(b.imageView + imageIndex);
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
                        barrierCount += MakeBarrierForLastUsage2(image);
                    }
                }
            }
        }
    }

    return barrierCount;
}
void VKCommandBufferProcessor::PreExecute(int inflightIndex, VKFramePrepareData& framePrepare)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor::PreExecute");

    auto& executedCmds = framePrepare.cmds;
    hoistedTransferCmdIndices.clear();
    hoistedTransferCmdIndices.reserve(16);

    // track where to put barriers
    for (int visitIndex = 0; visitIndex < executedCmds.size(); visitIndex++)
    {
        auto& cmd = executedCmds[visitIndex];
        if (cmd.type == VKCmdType::BeginRenderPass)
        {
            auto& args = std::get<VKBeginRenderPassCmd>(cmd.args);
            GoThroughRenderPass(
                inflightIndex,
                executedCmds,
                *args.renderPass,
                visitIndex,
                args.barrierCount,
                args.barrierOffset,
                args.hoistedTransferOffset,
                args.hoistedTransferCount
            );
        }
        else if (cmd.type == VKCmdType::AsyncReadback)
        {
            auto& args = std::get<VKAsyncReadbackCmd>(cmd.args);
            asyncReadbacks.push_back(std::move(*args.handle));
        }
        else if (cmd.type == VKCmdType::AllocateBuffer)
        {
            auto& args = std::get<VKAllocateBufferCmd>(cmd.args);
            AllocateTemporaryBufferCmd(args, inflightIndex);
        }
        else if (cmd.type == VKCmdType::RGBeginRenderPass)
        {
            auto& args = std::get<VKRGBeginRenderPassCmd>(cmd.args);

            auto renderPass = VKContext::Instance()->resourceAllocator->Request(args.renderPass);
            if (renderPass != nullptr)
            {
                GoThroughRenderPass(
                    inflightIndex,
                    executedCmds,
                    *renderPass,
                    visitIndex,
                    args.barrierCount,
                    args.barrierOffset,
                    args.hoistedTransferOffset,
                    args.hoistedTransferCount
                );
            }
            else
            {
                SPDLOG_ERROR("VKCommandBufferProcessor: failed to request render pass {}", args.renderPass.GetName());
            }
        }
        else if (cmd.type == VKCmdType::DynamicBeginRenderPass)
        {
            auto& args = std::get<VKDynamicRenderPassCmd>(cmd.args);
            auto& imgs = args.imageIdentifiers;

            std::optional<SubpassAttachment> depthAttachmentDescription = std::nullopt;
            auto lastImage = ImageIdentifier_GetImage(imgs.back().image, this);
            if (lastImage == nullptr)
            {
                SPDLOG_ERROR("VKCommandBufferProcessor: failed to resolve last image for DynamicBeginRenderPass");
                continue;
            }
            bool hasDepth = !imgs.empty() && IsDepthStencilFormat(lastImage->GetDescription().format);
            if (hasDepth)
            {
                depthAttachmentDescription = SubpassAttachment{
                    (int)imgs.size() - 1,
                    imgs.back().loadOp,
                    imgs.back().storeOp,
                    imgs.back().stencilLoadOp,
                    imgs.back().stencilStoreOp
                };
            }

            int colorAttachmentCount = imgs.size() - (hasDepth ? 1 : 0);
            int subpassCount = 1;

            RenderPass passDescriptor(subpassCount, imgs.size());

            std::vector<SubpassAttachment> colorAttachmentDescriptions(colorAttachmentCount);
            int idx = 0;
            std::transform(
                imgs.begin(),
                hasDepth ? imgs.end() - 1 : imgs.end(),
                colorAttachmentDescriptions.begin(),
                [&idx](RenderAttachment& atta)
                { return SubpassAttachment{idx++, atta.loadOp, atta.storeOp, atta.stencilLoadOp, atta.stencilStoreOp}; }
            );
            passDescriptor.SetSubpass(0, colorAttachmentDescriptions, depthAttachmentDescription);

            for (int attachmentIdx = 0; attachmentIdx < (idx + (hasDepth ? 1 : 0)); ++attachmentIdx)
            {
                passDescriptor.SetAttachment(attachmentIdx, imgs[attachmentIdx].image);
            }

            VKRenderPass* renderPass = Request(passDescriptor);
            args.resolvedRenderPass = renderPass;
            GoThroughRenderPass(
                inflightIndex,
                executedCmds,
                *renderPass,
                visitIndex,
                args.barrierCount,
                args.barrierOffset,
                args.hoistedTransferOffset,
                args.hoistedTransferCount
            );
        }
        else if (cmd.type == VKCmdType::BindResource)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: bind resource");
            auto& args = std::get<VKBindResourceCmd>(cmd.args);
            recordState.bindSetCmdIndex[args.set] = visitIndex;
            recordState.bindedSetUpdateNeeded[args.set] = true;
        }
        else if (cmd.type == VKCmdType::DynamicBindResource)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: dynamic bind resource");
            auto& args = std::get<VKDynamicBindResourceCmd>(cmd.args);
            recordState.dynamicBindSetCmdIndex[args.set] = visitIndex;
            recordState.dynamicBindedSetUpdateNeeded[args.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            ScheduleBindShaderProgram(cmd, visitIndex);
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: copy buffer");
            auto& args = std::get<VKCopyBufferCmd>(cmd.args);
            PrepareCopyBufferCmd(args, inflightIndex);
        }
        else if (cmd.type == VKCmdType::UploadData)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: upload data");
            auto& args = std::get<VKUploadDataCmd>(cmd.args);
            PrepareUploadDataCmd(args, inflightIndex);
        }
        else if (cmd.type == VKCmdType::Blit)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: blit");
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            auto& args = std::get<VKBlitCmd>(cmd.args);
            if (args.from == nullptr || args.to == nullptr)
            {
                SPDLOG_ERROR("VKCommandBufferProcessor: blit source or destination is null");
                continue;
            }
            Gfx::ImageSubresourceRange srcRange{
                .aspectMask = Gfx::MapVKImageAspect(args.to->GetDefaultSubresourceRange().aspectMask),
                .baseMipLevel = args.blitOp.srcMip.value_or(0),
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = args.to->GetDefaultSubresourceRange().layerCount,
            };

            if (TrackResource(
                    args.from,
                    srcRange,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT
                ))
                barrierCount += MakeBarrierForLastUsage2(args.from);

            Gfx::ImageSubresourceRange dstRange{
                .aspectMask = Gfx::MapVKImageAspect(args.to->GetDefaultSubresourceRange().aspectMask),
                .baseMipLevel = args.blitOp.dstMip.value_or(0),
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = args.to->GetDefaultSubresourceRange().layerCount,
            };

            if (TrackResource(
                    args.to,
                    dstRange,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_ACCESS_TRANSFER_WRITE_BIT
                ))
                barrierCount += MakeBarrierForLastUsage2(args.to);

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::CopyImageToBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: copy image to buffer");
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            auto& args = std::get<VKCopyImageToBufferCmd>(cmd.args);
            if (args.dst.type == BufferIdentifier::Type::RawBuffer)
            {
                auto refDst = ResolveTrackableBuffer(args.dst, inflightIndex);
                if (TrackResource(refDst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
                    barrierCount += MakeBarrierForLastUsage(refDst);
            }

            for (int i = 0; i < args.regionsCount; ++i)
            {
                auto& subresource = args.regions[i].layers;
                Gfx::ImageSubresourceRange range{
                    .aspectMask = subresource.aspectMask,
                    .baseMipLevel = subresource.mipLevel,
                    .levelCount = 1,
                    .baseArrayLayer = subresource.baseArrayLayer,
                    .layerCount = subresource.layerCount,
                };

                if (TrackResource(
                        args.src,
                        range,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_TRANSFER_READ_BIT
                    ))
                    barrierCount += MakeBarrierForLastUsage2(args.src);
            }

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::CopyBufferToImage)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: copy buffer to image");
            auto& args = std::get<VKCopyBufferToImageCmd>(cmd.args);
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            if (args.src.type == BufferIdentifier::Type::RawBuffer)
            {
                auto refSrc = ResolveTrackableBuffer(args.src, inflightIndex);
                if (TrackResource(refSrc, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                    barrierCount += MakeBarrierForLastUsage(refSrc);
            }

            for (int i = 0; i < args.regionCount; ++i)
            {
                auto& subresource = args.regions[i].imageSubresource;
                Gfx::ImageSubresourceRange range{
                    .aspectMask = Gfx::MapVKImageAspect(subresource.aspectMask),
                    .baseMipLevel = subresource.mipLevel,
                    .levelCount = 1,
                    .baseArrayLayer = subresource.baseArrayLayer,
                    .layerCount = subresource.layerCount,
                };

                if (TrackResource(
                        args.dst,
                        range,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT
                    ))
                    barrierCount += MakeBarrierForLastUsage2(args.dst);
            }

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: push descriptor set");
            TrackResourceForPushDescriptorSet(cmd, false);
        }
        else if (cmd.type == VKCmdType::Present)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: present");
            auto& args = std::get<VKPresentCmd>(cmd.args);
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
                    args.image,
                    range,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_NONE
                ))
            {
                barrierCount += MakeBarrierForLastUsage2(args.image);
            }
            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::SetTexture)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: set texture");
            auto& args = std::get<VKSetTextureCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {ResourceType::Image, ObjPtr<Image>(args.image), args.imageViewOption};
        }
        else if (cmd.type == VKCmdType::SetBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: set buffer");
            auto& args = std::get<VKSetBufferCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {ResourceType::Buffer, args.buffer, std::nullopt};
        }
        else if (cmd.type == VKCmdType::AllocateAttachment)
        {
            // done in command buffer
        }
        else if (cmd.type == VKCmdType::Dispatch)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: dispatch");
            std::vector<VKImage*> list;
            auto& args = std::get<VKDispatchCmd>(cmd.args);
            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            MakeBarrierForAllDynamicBindedSetUpdate(executedCmds, list, args.barrierCount);
            FlushAllBindedSetUpdate(inflightIndex, executedCmds, list, args.barrierCount);
        }
        else if (cmd.type == VKCmdType::DispatchIndirect)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: dispatchIndir");
            auto& args = std::get<VKDispatchIndirectCmd>(cmd.args);
            std::vector<VKImage*> list;
            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            if (args.buffer.type == BufferIdentifier::Type::RawBuffer)
            {
                auto refBuf = ResolveTrackableBuffer(args.buffer, inflightIndex);
                if (TrackResource(refBuf, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT))
                    args.barrierCount += MakeBarrierForLastUsage(refBuf);
            }
            MakeBarrierForAllDynamicBindedSetUpdate(executedCmds, list, args.barrierCount);
            FlushAllBindedSetUpdate(inflightIndex, executedCmds, list, args.barrierCount);
        }
        else if (cmd.type == VKCmdType::ClearColorImage)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: clear color image");
            auto& args = std::get<VKClearColorImageCmd>(cmd.args);
            auto image = static_cast<VKImage*>(args.image);
            Gfx::ImageSubresourceRange range{
                .aspectMask = ImageAspectFlags::Color,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            };

            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            if (image && TrackResource(
                             image,
                             range,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_ACCESS_TRANSFER_WRITE_BIT
                         ))
            {
                args.barrierCount += MakeBarrierForLastUsage2(image);
            }
        }
    }
}

void VKCommandBufferProcessor::Execute(
    VKFramePrepareData& framePrepare,
    VKFrameContext& inflightCmd,
    int inflightIndex,
    Queue& executionQueue,
    const GfxFeaturesSettings& featureSettings,
    CmdBufExecutionReport& report
)
{
    PreExecute(inflightIndex, framePrepare);
    auto& executedCmds = framePrepare.cmds;

    report = CmdBufExecutionReport(); // reset report

    VkCommandBuffer vkcmd = inflightCmd.cmd;

    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor::Execute");

    // Begin
    bool enableGPUTimestamp = inflightCmd.maxtimestapQueryCount > 0 && featureSettings.enableGPUProfiling;
    if (enableGPUTimestamp)
    {
        vkCmdResetQueryPool(vkcmd, inflightCmd.timestapQueryPool, 0, inflightCmd.maxtimestapQueryCount);
        exeState.currentTimestapQueryIndex = 0;
    }

    bool insideRenderPass = false;
    for (size_t i = 0; i < executedCmds.size(); ++i)
    {
        auto& cmd = executedCmds[i];
        if (insideRenderPass && IsHoistedRenderPassTransferCmd(cmd.type))
        {
            continue;
        }
        if (insideRenderPass && IsUnsupportedRenderPassTransferCmd(cmd.type))
        {
            SPDLOG_ERROR("VKCommandBufferProcessor: skipped unsupported transfer command inside render pass");
            continue;
        }

        switch (cmd.type)
        {
            case VKCmdType::SetLineWidth:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetLineWidth");
                    auto& args = std::get<VKSetLineWidthCmd>(cmd.args);
                    vkCmdSetLineWidth(vkcmd, args.lineWidth);
                    break;
                }
            case VKCmdType::SetDepthBias:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetDepthBias");
                    auto& args = std::get<VKSetDepthBiasCmd>(cmd.args);
                    vkCmdSetDepthBias(vkcmd, args.constantFactor, args.clamp, args.slopeFactor);
                    break;
                }
            case VKCmdType::SetDepthBiasEnable:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetDepthBiasEnable");
                    auto& args = std::get<VKSetDepthBiasEnableCmd>(cmd.args);
                    vkCmdSetDepthBiasEnable(vkcmd, args.enable ? VK_TRUE : VK_FALSE);
                    break;
                }
            case VKCmdType::DrawIndexed:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DrawIndexed");
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    auto& args = std::get<VKDrawIndexedCmd>(cmd.args);
                    vkCmdDrawIndexed(
                        vkcmd,
                        args.indexCount,
                        args.instanceCount,
                        args.firstIndex,
                        args.vertexOffset,
                        args.firstInstance
                    );
                    break;
                }
            case VKCmdType::Draw:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - Draw");
                    auto& args = std::get<VKDrawCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    vkCmdDraw(vkcmd, args.vertexCount, args.instanceCount, args.firstVertex, args.firstInstance);
                    break;
                }
            case Gfx::VKCmdType::ClearColorImage:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - ClearColorImage");
                    auto& args = std::get<VKClearColorImageCmd>(cmd.args);

                    VKImage* image = static_cast<VKImage*>(args.image);

                    VkImage vkimage = image->GetImage();
                    VkClearColorValue clearValue;
                    memcpy(&clearValue, &args.clearValue, sizeof(VkClearColorValue));
                    VkImageSubresourceRange range{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    };

                    int barrierOffset = args.barrierOffset;
                    int barrierCount = args.barrierCount;
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    vkCmdClearColorImage(vkcmd, vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
                    break;
                }
            case VKCmdType::BuildBLAS:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BuildBLAS");
                    auto& args = std::get<VKBuildBLASCmd>(cmd.args);
                    rayTracingManager->CreateBLASCommandBufferImpl(vkcmd, args.handle, args.vkGeometries, args.maxPrimitiveCounts);
                    break;
                }
            case VKCmdType::BuildTLAS:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BuildTLAS");
                    auto& args = std::get<VKBuildTLASCmd>(cmd.args);
                    rayTracingManager->BuildSceneCommandBufferImpl(vkcmd, args.handle, args.instances);
                    break;
                }
            case VKCmdType::DrawIndirect:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DrawIndirect");
                    auto& args = std::get<VKDrawIndirectCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    auto resolved = ResolveBuffer(args.buffer, inflightIndex);
                    vkCmdDrawIndirect(
                        vkcmd,
                        resolved.buffer,
                        args.offset + resolved.offset,
                        args.drawCount,
                        args.stride
                    );
                    break;
                }
            case VKCmdType::DrawIndexedIndirect:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DrawIndexedIndirect");
                    auto& args = std::get<VKDrawIndexedIndirectCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    auto resolved = ResolveBuffer(args.buffer, inflightIndex);
                    vkCmdDrawIndexedIndirect(
                        vkcmd,
                        resolved.buffer,
                        args.offset + resolved.offset,
                        args.drawCount,
                        args.stride
                    );
                    break;
                }
            case VKCmdType::AsyncReadback:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - AsyncReadback");
                    // vkCmdSetEvent(vkcmd, cmd.asyncReadback.event, VK_PIPELINE_STAGE_TRANSFER_BIT);
                    break;
                }
            case Gfx::VKCmdType::DynamicBeginRenderPass:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DynamicBeginRenderPass");
                    auto& args = std::get<VKDynamicRenderPassCmd>(cmd.args);
                    VKRenderPass* renderPasss = args.resolvedRenderPass;
                    std::span<ClearValue> clearValues = args.clearValues;

                    ExecuteHoistedRenderPassTransfers(
                        vkcmd,
                        executedCmds,
                        args.hoistedTransferOffset,
                        args.hoistedTransferCount,
                        inflightIndex
                    );

                    auto& subpasses = renderPasss->GetSubpesses();
                    size_t clearCount = subpasses[0].colors.size() + (subpasses[0].depth.has_value() ? 1 : 0);
                    std::vector<VkClearValue> clearValuesFinal(clearCount, {{{0, 0, 0, 0}}});

                    ASSERT(clearValues.size() == clearValuesFinal.size());

                    for (int i = 0; i < clearValues.size() && i < clearValuesFinal.size(); i++)
                    {
                        memcpy(&clearValuesFinal[i], &clearValues[i], sizeof(VkClearColorValue));
                    }

                    BeginRenderPass(
                        vkcmd,
                        renderPasss,
                        clearValuesFinal.data(),
                        clearValuesFinal.size(),
                        args.barrierOffset,
                        args.barrierCount
                    );
                    insideRenderPass = true;
                    break;
                }
            case VKCmdType::BeginRenderPass:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BeginRenderPass");
                    auto& args = std::get<VKBeginRenderPassCmd>(cmd.args);
                    Gfx::VKRenderPass* renderPass = args.renderPass;
                    VkRenderPass vkRenderPass = renderPass->GetHandle();
                    exeState.renderPass = renderPass;
                    exeState.subpassIndex = 0;

                    ExecuteHoistedRenderPassTransfers(
                        vkcmd,
                        executedCmds,
                        args.hoistedTransferOffset,
                        args.hoistedTransferCount,
                        inflightIndex
                    );

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
                    renderPassBeginInfo.clearValueCount = args.clearValueCount;
                    renderPassBeginInfo.pClearValues = args.clearValues;

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    VkViewport viewport;
                    viewport.x = 0.0f;
                    viewport.y = 0.0f;
                    viewport.width = (float)extent.width;
                    viewport.height = (float)extent.height;
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;

                    VkRect2D scissor;
                    scissor.offset = {0, 0};
                    scissor.extent = {extent.width, extent.height};

                    UpdateViewportAndScissorForRenderPass(vkcmd, viewport, scissor, extent);
                    vkCmdBeginRenderPass(vkcmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
                    insideRenderPass = true;
                    break;
                }
            case VKCmdType::EndRenderPass:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - EndRenderPass");
                    vkCmdEndRenderPass(vkcmd);

                    exeState.overrideScissor = false;
                    exeState.overrideViewport = false;
                    exeState.renderPass = nullptr;
                    insideRenderPass = false;
                    break;
                }
            case VKCmdType::Blit:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - Blit");
                    auto& args = std::get<VKBlitCmd>(cmd.args);
                    uint32_t srcMip = args.blitOp.srcMip.value_or(0);
                    uint32_t dstMip = args.blitOp.dstMip.value_or(0);
                    VkImageBlit blit;
                    blit.dstOffsets[0] = {0, 0, 0};
                    blit.dstOffsets[1] = {
                        (int32_t)(args.to->GetDescription().width / glm::pow(2, dstMip)),
                        (int32_t)(args.to->GetDescription().height / glm::pow(2, dstMip)),
                        1
                    };
                    VkImageSubresourceLayers dstLayers;
                    dstLayers.aspectMask = args.to->GetDefaultSubresourceRange().aspectMask;
                    dstLayers.baseArrayLayer = 0;
                    dstLayers.layerCount = args.to->GetDefaultSubresourceRange().layerCount;
                    dstLayers.mipLevel = dstMip;
                    blit.dstSubresource = dstLayers;

                    blit.srcOffsets[0] = {0, 0, 0};
                    blit.srcOffsets[1] = {
                        (int32_t)(args.from->GetDescription().width / glm::pow(2, srcMip)),
                        (int32_t)(args.from->GetDescription().height / glm::pow(2, srcMip)),
                        1
                    };
                    VkImageSubresourceLayers srcLayers = dstLayers;
                    srcLayers.mipLevel = args.blitOp.srcMip.value_or(0);
                    blit.srcSubresource = srcLayers; // basically copy the resources from dst
                                                     // without much configuration

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    vkCmdBlitImage(
                        vkcmd,
                        args.from->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        args.to->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit,
                        VK_FILTER_NEAREST
                    );
                    break;
                }
            case VKCmdType::BindResource:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BindResource");
                    auto& args = std::get<VKBindResourceCmd>(cmd.args);
                    if (args.set > 4)
                        return;

                    exeState.setResources[args.set].resource = (VKShaderResource*)args.resource;
                    exeState.setResources[args.set].needUpdate = true;
                    break;
                }
            case VKCmdType::DynamicBindResource:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DynamicBindResource");
                    auto& args = std::get<VKDynamicBindResourceCmd>(cmd.args);
                    if (args.set > 4)
                        return;

                    exeState.setResources[args.set].resource = nullptr;
                    exeState.setResources[args.set].dynamicBindSetCmdIndex = i;
                    exeState.setResources[args.set].dynamicBindingNeedUpdate = true;
                    break;
                }
            case VKCmdType::BindVertexBuffer:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BindVertexBuffer");
                    auto& args = std::get<VKBindVertexBufferCmd>(cmd.args);
                    VkBuffer vkBuffers[8];
                    uint64_t vkOffsets[8];
                    for (uint32_t i = 0; i < args.vertexBufferBindingCount; ++i)
                    {
                        auto resolved = ResolveBuffer(args.vertexBufferBindings[i].buffer, inflightIndex);
                        exeState.vertexBufferBindings[i] = static_cast<VKBuffer*>(args.vertexBufferBindings[i].buffer.buffer);
                        vkBuffers[i] = resolved.buffer;
                        vkOffsets[i] = args.vertexBufferBindings[i].offset + resolved.offset;
                    }
                    exeState.vertexBufferBindingCount = args.vertexBufferBindingCount;

                    vkCmdBindVertexBuffers(
                        vkcmd,
                        args.firstBindingIndex,
                        args.vertexBufferBindingCount,
                        vkBuffers,
                        vkOffsets
                    );
                    break;
                }
            case VKCmdType::BindShaderProgram:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BindShaderProgram");
                    auto& args = std::get<VKBindShaderProgramCmd>(cmd.args);
                    exeState.pendingBindedShader = args.program;
                    exeState.pipelineConfig = *args.config;
                    break;
                }
            case VKCmdType::BindIndexBuffer:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BindIndexBuffer");
                    auto& args = std::get<VKBindIndexBufferCmd>(cmd.args);
                    auto resolved = ResolveBuffer(args.buffer, inflightIndex);
                    vkCmdBindIndexBuffer(vkcmd, resolved.buffer, args.offset + resolved.offset, args.indexType);
                    break;
                }
            case VKCmdType::SetViewport:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetViewport");
                    auto& args = std::get<VKSetViewportCmd>(cmd.args);
                    exeState.overrideViewport = true;
                    vkCmdSetViewport(vkcmd, 0, 1, &args.viewport);
                    break;
                }
            case VKCmdType::CopyImageToBuffer:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - CopyImageToBuffer");
                    std::vector<VkBufferImageCopy> vkRegions;
                    auto& args = std::get<VKCopyImageToBufferCmd>(cmd.args);

                    auto resolvedDst = ResolveBuffer(args.dst, inflightIndex);

                    for (int i = 0; i < args.regionsCount; ++i)
                    {
                        auto& r = args.regions[i];
                        VkBufferImageCopy region;
                        region.bufferOffset = r.bufferOffset + resolvedDst.offset;
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

                    VKImage* srcImage = args.src;

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    vkCmdCopyImageToBuffer(
                        vkcmd,
                        srcImage->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        resolvedDst.buffer,
                        vkRegions.size(),
                        vkRegions.data()
                    );
                    break;
                }
            case VKCmdType::SetPushConstant:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetPushConstant");
                    auto& args = std::get<VKSetPushConstantCmd>(cmd.args);
                    VKShaderProgram* shaderProgram = args.shaderProgram;

                    vkCmdPushConstants(
                        vkcmd,
                        shaderProgram->GetVKPipelineLayout(),
                        args.stages,
                        0,
                        args.dataSize,
                        args.data
                    );
                    break;
                }
            case VKCmdType::SetScissor:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - SetScissor");
                    auto& args = std::get<VKSetScissorCmd>(cmd.args);
                    exeState.overrideScissor = true;
                    vkCmdSetScissor(vkcmd, args.firstScissor, args.scissorCount, args.rects);
                    break;
                }
            case VKCmdType::Dispatch:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - Dispatch");
                    auto& args = std::get<VKDispatchCmd>(cmd.args);
                    const auto pipeineBindingPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, pipeineBindingPoint);

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    vkCmdDispatch(vkcmd, args.groupCountX, args.groupCountY, args.groupCountZ);
                    break;
                }
            case VKCmdType::DispatchIndirect:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - DispatchIndirect");
                    auto& args = std::get<VKDispatchIndirectCmd>(cmd.args);
                    const auto pipeineBindingPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(executedCmds, inflightIndex, vkcmd, pipeineBindingPoint);

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    PutBarriers(vkcmd, barrierOffset, barrierCount);
                    auto resolved = ResolveBuffer(args.buffer, inflightIndex);
                    vkCmdDispatchIndirect(vkcmd, resolved.buffer, args.bufferOffset + resolved.offset);
                    break;
                }
            case VKCmdType::NextRenderPass:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - NextRenderPass");
                    exeState.subpassIndex += 1;
                    vkCmdNextSubpass(vkcmd, VK_SUBPASS_CONTENTS_INLINE);
                    break;
                }
            case VKCmdType::PushDescriptorSet:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - PushDescriptorSet");
                    auto& args = std::get<VKPushDescriptorCmd>(cmd.args);
                    VkWriteDescriptorSet writeSets[8];
                    VkDescriptorImageInfo imageInfos[16];

                    int i = 0;
                    int imageIndex = 0;
                    for (int bindingIndex = 0; bindingIndex < args.bindingCount; ++bindingIndex)
                    {
                        auto& b = args.bindings[bindingIndex];
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
                                .dstBinding = (uint32_t)b.dstBinding,
                                .dstArrayElement = (uint32_t)b.dstArrayElement,
                                .descriptorCount = (uint32_t)b.descriptorCount,
                                .descriptorType =
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // this should be queried from shader,
                                                                               // but shader currently don't support an
                                                                               // easy way to get the binding type (need
                                                                               // iterate through ShaderInfo.bindings)
                                .pImageInfo = imageInfos,
                            };
                            i += 1;
                        }
                    }

                    vkCmdPushDescriptorSetKHR(
                        vkcmd,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        args.shader->GetVKPipelineLayout(),
                        args.set,
                        args.bindingCount,
                        writeSets
                    );

                    break;
                }
            case VKCmdType::CopyBuffer:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - CopyBuffer");
                    auto& args = std::get<VKCopyBufferCmd>(cmd.args);
                    ExecuteCopyBufferCmd(vkcmd, args, inflightIndex);
                    break;
                }
            case VKCmdType::UploadData:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - UploadData");
                    auto& args = std::get<VKUploadDataCmd>(cmd.args);
                    ExecuteUploadDataCmd(vkcmd, args, inflightIndex);
                    break;
                }
            case VKCmdType::CopyBufferToImage:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - CopyBufferToImage");
                    auto& args = std::get<VKCopyBufferToImageCmd>(cmd.args);
                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    PutBarriers(vkcmd, barrierOffset, barrierCount);

                    auto resolvedSrc = ResolveBuffer(args.src, inflightIndex);
                    VkBufferImageCopy adjustedRegions[8];
                    for (uint32_t i = 0; i < args.regionCount; ++i)
                    {
                        adjustedRegions[i] = args.regions[i];
                        adjustedRegions[i].bufferOffset += resolvedSrc.offset;
                    }
                    vkCmdCopyBufferToImage(
                        vkcmd,
                        resolvedSrc.buffer,
                        args.dst->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        args.regionCount,
                        adjustedRegions
                    );
                    break;
                }
            case VKCmdType::Present:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - Present");
                    auto& args = std::get<VKPresentCmd>(cmd.args);
                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    // {
                    //     PutBarrier(vkcmd, b);
                    // }
                    PutBarriers(vkcmd, barrierOffset, barrierCount);
                    break;
                }

            case VKCmdType::RGBeginRenderPass:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - RGBeginRenderPass");
                    auto& args = std::get<VKRGBeginRenderPassCmd>(cmd.args);
                    Gfx::VKRenderPass* renderPass = VKContext::Instance()->resourceAllocator->Request(args.renderPass);
                    ExecuteHoistedRenderPassTransfers(
                        vkcmd,
                        executedCmds,
                        args.hoistedTransferOffset,
                        args.hoistedTransferCount,
                        inflightIndex
                    );
                    BeginRenderPass(
                        vkcmd,
                        renderPass,
                        args.clearValues,
                        args.clearValueCount,
                        args.barrierOffset,
                        args.barrierCount
                    );
                    insideRenderPass = true;
                    break;
                }
            case Gfx::VKCmdType::BeginLabel:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - BeginLabel");
                    auto& args = std::get<VKBeginLabelCmd>(cmd.args);
                    VKDebugUtils::CmdBeginLabel(vkcmd, args.label.data(), args.color);
                    if (enableGPUTimestamp && exeState.currentTimestapQueryIndex < inflightCmd.maxtimestapQueryCount)
                    {
                        vkCmdWriteTimestamp(
                            vkcmd,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            inflightCmd.timestapQueryPool,
                            exeState.currentTimestapQueryIndex
                        );
                        exeState.currentTimestapQueryIndex += 1;
                        report.timestampQueryLabels.push_back({TimestampLabelType::Begin, args.label});
                    }
                    break;
                }
            case Gfx::VKCmdType::EndLabel:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - EndLabel");
                    VKDebugUtils::CmdEndLabel(vkcmd);
                    if (enableGPUTimestamp && exeState.currentTimestapQueryIndex < inflightCmd.maxtimestapQueryCount)
                    {
                        vkCmdWriteTimestamp(
                            vkcmd,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            inflightCmd.timestapQueryPool,
                            exeState.currentTimestapQueryIndex
                        );
                        exeState.currentTimestapQueryIndex += 1;
                        report.timestampQueryLabels.push_back({TimestampLabelType::End, ""});
                    }
                    break;
                }
            case Gfx::VKCmdType::InsertLabel:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - InsertLabel");
                    auto& args = std::get<VKInsertLabelCmd>(cmd.args);
                    VKDebugUtils::CmdInsertLabel(vkcmd, args.label.data(), args.color);
                    break;
                }
            case Gfx::VKCmdType::GraphicsBlit:
                {
                    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - GraphicsBlit");
                    auto& args = std::get<VKGraphicsBlitCmd>(cmd.args);
                    VkRenderingAttachmentInfo colorAttachmentInfo =
                        VkRenderingAttachmentInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
                    colorAttachmentInfo.imageView = ImageIdentifier_GetImageView(args.from, this)->GetHandle();
                    const auto& desc = args.from.GetAsImage()->GetDescription();

                    auto renderArea = VkRect2D{VkOffset2D{}, VkExtent2D{desc.width, desc.height}};
                    VkRenderingInfo renderInfo = VkRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
                    renderInfo.renderArea = renderArea;
                    renderInfo.pColorAttachments = &colorAttachmentInfo;
                    renderInfo.layerCount = 1;

                    // Or use ResourceAllocator for RenderPass
                    vkCmdBeginRendering(vkcmd, &renderInfo);

                    vkCmdEndRendering(vkcmd);

                    break;
                }
            case VKCmdType::None: break;
        }
    }

    for (auto& r : resourceUsageTracks)
    {
        std::swap(r.second.currentFrameUsages, r.second.previousFrameUsages);
        r.second.currentFrameUsages.clear();
    }
    exeState = ExecutionState();
    recordState = RecordState();

    imageMemoryBarriers.clear();
    bufferMemoryBarriers.clear();
    memoryBarriers.clear();
    barriers.clear();

    VKContext::Instance()->resourceAllocator->Tick();
}

void VKCommandBufferProcessor::UpdateDynamicDescriptorSetBinding(std::vector<VKCmd>& cmds, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, int inflightIndex, int setIndex)
{
    if (exeState.setResources[setIndex].dynamicBindingNeedUpdate)
    {
        if (exeState.setResources[setIndex].resource == nullptr)
        {
            exeState.setResources[setIndex].dynamicBindingNeedUpdate = false;

            VKDynamicBindResourceCmd& dynamicBindResourceCmd = std::get<VKDynamicBindResourceCmd>(cmds[exeState.setResources[setIndex].dynamicBindSetCmdIndex].args);
            BindDynamicDescriptorSet(cmd, bindPoint, dynamicBindResourceCmd, setIndex, exeState.bindedShader, inflightIndex);
        }
    }
}

void VKCommandBufferProcessor::UpdateDescriptorSetBinding(
    std::vector<VKCmd>& cmds, int inflightIndex, VkCommandBuffer cmd, VkPipelineBindPoint bindPoint
)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - UpdateDescriptorSetBinding");

    for (int setIndex = 0; setIndex < 4; ++setIndex)
    {
        UpdateDescriptorSetBinding(inflightIndex, cmd, setIndex, bindPoint);
        UpdateDynamicDescriptorSetBinding(cmds, cmd, bindPoint, inflightIndex, setIndex);
    }
}

void VKCommandBufferProcessor::TryBindShader(VkCommandBuffer cmd)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - TryBindShader");

    if ((exeState.bindedShader != exeState.pendingBindedShader || exeState.pipelineConfig != exeState.pendingPipelineConfig) &&
        exeState.pendingBindedShader != nullptr)
    {
        bool requirePushDescriptorSet = false;
        for (int setIndex = 0; setIndex < 4; ++setIndex)
        {
            requirePushDescriptorSet = requirePushDescriptorSet || exeState.setResources[setIndex].dynamicBindingNeedUpdate;
        }

        if (exeState.pendingBindedShader->IsCompute())
        {
            auto pipeline = exeState.pendingBindedShader->RequestComputePipeline(requirePushDescriptorSet);

            if (pipeline != exeState.lastBindedPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                exeState.lastBindedPipeline = pipeline;
                exeState.bindedShader = exeState.pendingBindedShader;
                exeState.pipelineConfig = exeState.pendingPipelineConfig;

                for (int setIndex = 0; setIndex < 4; ++setIndex)
                {
                    exeState.setResources[setIndex].needUpdate = exeState.setResources[setIndex].resource != nullptr;
                    exeState.bindedDescriptorSets[setIndex] = VK_NULL_HANDLE;
                }
            }
        }
        else
        {
            // binding pipeline
            ASSERT(exeState.renderPass != nullptr && "RenderPass is null, draw call maybe not inside a RenderPass");

            if (exeState.renderPass != nullptr)
            {
                auto pipeline = exeState.pendingBindedShader->RequestGraphicsPipeline(
                    exeState.pipelineConfig,
                    std::span<VKBuffer*>(exeState.vertexBufferBindings, exeState.vertexBufferBindingCount),
                    exeState.renderPass,
                    exeState.subpassIndex,
                    requirePushDescriptorSet
                );

                if (pipeline != exeState.lastBindedPipeline)
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    exeState.lastBindedPipeline = pipeline;
                    exeState.bindedShader = exeState.pendingBindedShader;
                    exeState.pipelineConfig = exeState.pendingPipelineConfig;

                    for (int setIndex = 0; setIndex < 4; ++setIndex)
                    {
                        exeState.setResources[setIndex].needUpdate = exeState.setResources[setIndex].resource != nullptr;
                        exeState.bindedDescriptorSets[setIndex] = VK_NULL_HANDLE;
                    }
                }
            }
            else
                spdlog::error("draw call outside of renderpass");
        }
    }
}

void VKCommandBufferProcessor::UpdateDescriptorSetBinding(int inflightIndex, VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - UpdateDescriptorSetBinding");

    if (exeState.setResources[index].needUpdate)
    {
        if (exeState.setResources[index].resource)
        {
            auto sourceSet =
                exeState.setResources[index].resource->GetDescriptorSet(inflightIndex, index, exeState.bindedShader, this);
            if (sourceSet != VK_NULL_HANDLE && sourceSet != exeState.bindedDescriptorSets[index])
            {
                vkCmdBindDescriptorSets(
                    cmd,
                    bindPoint,
                    exeState.bindedShader->GetVKPipelineLayout(),
                    index,
                    1,
                    &sourceSet,
                    0,
                    VK_NULL_HANDLE
                );

                exeState.bindedDescriptorSets[index] = sourceSet;
                exeState.setResources[index].needUpdate = false;
            }
        }
    }
}

void VKCommandBufferProcessor::PutBarriers(VkCommandBuffer vkcmd, int barrierOffset, int barrierCount)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - PutBarriers2");

    std::vector<VkImageMemoryBarrier2> imageBarriers{};
    std::vector<VkBufferMemoryBarrier2> bufferBarriers{};
    std::vector<VkMemoryBarrier2> memoryBarrier2s{};

    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
    {
        Barrier& barrier = barriers[b];
        if (barrier.bufferMemoryBarrierIndex != -1)
        {
            bufferBarriers.push_back(
                VkBufferMemoryBarrier2{
                    VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    VK_NULL_HANDLE,
                    barrier.srcStageMask,
                    bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex].srcAccessMask,
                    barrier.dstStageMask,
                    bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex].dstAccessMask,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex].buffer,
                    bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex].offset,
                    bufferMemoryBarriers[barrier.bufferMemoryBarrierIndex].size
                }
            );
        }
        else if (barrier.imageMemorybarrierIndex != -1)
        {
            imageBarriers.push_back(
                VkImageMemoryBarrier2{
                    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                    VK_NULL_HANDLE,
                    barrier.srcStageMask,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].srcAccessMask,
                    barrier.dstStageMask,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].dstAccessMask,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].oldLayout,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].newLayout,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].image,
                    imageMemoryBarriers[barrier.imageMemorybarrierIndex].subresourceRange
                }
            );
        }
        else if (barrier.memoryBarrierIndex != -1)
        {
            memoryBarrier2s.push_back(
                VkMemoryBarrier2{
                    VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                    VK_NULL_HANDLE,
                    barrier.srcStageMask,
                    memoryBarriers[barrier.memoryBarrierIndex].srcAccessMask,
                    barrier.dstStageMask,
                    memoryBarriers[barrier.memoryBarrierIndex].dstAccessMask
                }
            );
        }
    }

    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.bufferMemoryBarrierCount = bufferBarriers.size();
    dependencyInfo.pBufferMemoryBarriers = bufferBarriers.data();
    dependencyInfo.imageMemoryBarrierCount = imageBarriers.size();
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
    dependencyInfo.memoryBarrierCount = memoryBarrier2s.size();
    dependencyInfo.pMemoryBarriers = memoryBarrier2s.data();

    vkCmdPipelineBarrier2(vkcmd, &dependencyInfo);
}

void VKCommandBufferProcessor::PutBarrier(VkCommandBuffer vkcmd, int index)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - PutBarriers");

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

void VKCommandBufferProcessor::ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex)
{
    recordState.bindProgramIndex = visitIndex;
}

VKImage* VKCommandBufferProcessor::GetImage(const UUID& hash)
{
    return VKContext::Instance()->resourceAllocator->GetImage(hash);
}

VKCommandBufferProcessor::VKCommandBufferProcessor(int inflightCount, VKRayTracing::Manager* rayTracingManager)
{
    this->rayTracingManager = rayTracingManager;
    temporaryBuffers.resize(inflightCount);
    transientDescriptorSets.resize(inflightCount);

    Buffer::CreateInfo createInfo{
        .usages = BufferUsage::Storage | BufferUsage::Transfer_Dst,
        .size = 1,
        .visibleInCPU = false,
        .debugName = "Default Buffer"
    };
    defaultSSBOBuffer = std::make_unique<VKBuffer>(createInfo);

    createInfo.usages = BufferUsage::Uniform | BufferUsage::Transfer_Dst;
    defaultUBOBuffer = std::make_unique<VKBuffer>(createInfo);
}

void VKCommandBufferProcessor::ShaderReloaded()
{
    descriptorSetCache.clear();
}

VKCommandBufferProcessor::~VKCommandBufferProcessor()
{
    for (auto& d : descriptorSetCache)
    {
        if (d.second.shaderProgram)
        {
            d.second.shaderProgram->GetDescriptorPool(d.second.setIndex)->Deallocate(d.second.set);
        }
    }
}

void VKCommandBufferProcessor::MakeBarrierFromWritableResources(std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded, const std::vector<VKWritableGPUResource>& writableResources)
{
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
            VKImage* data = static_cast<VKImage*>(std::get<ObjPtr<Image>>(w.data).Get());

            if (data == nullptr ||
                std::find(shaderImageSampleIgnoreList.begin(), shaderImageSampleIgnoreList.end(), data) !=
                    shaderImageSampleIgnoreList.end())
            {
                continue;
            }

            if (TrackResource(
                    data,
                    w.imageView ? w.imageView->GetSubresourceRange() : data->GetSubresourceRange(),
                    w.layout,
                    w.stages,
                    w.access
                ))
            {
                barrierCountAdded += MakeBarrierForLastUsage2(data);
            }
        }
        else
        {
            VKBuffer* data = static_cast<VKBuffer*>(std::get<ObjPtr<Buffer>>(w.data).Get());
            if (data == nullptr)
                continue;

            if (TrackResource((VKBuffer*)data, w.stages, w.access))
            {

                barrierCountAdded += MakeBarrierForLastUsage(data, data->GetUUID());
            }
        }
    }
}
void VKCommandBufferProcessor::MakeBarrierForAllDynamicBindedSetUpdate(
    std::vector<VKCmd>& cmds, std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded
)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - FlushAllDynamicBindedSetUpdate");

    for (int i = 0; i < 4; ++i)
    {
        if (recordState.dynamicBindedSetUpdateNeeded[i] == true)
        {
            recordState.dynamicBindedSetUpdateNeeded[i] = false;
            auto bindSetCmdIndex = recordState.dynamicBindSetCmdIndex[i];
            auto bindProgramIndex = recordState.bindProgramIndex;
            auto& bindProgramArgs = std::get<VKBindShaderProgramCmd>(cmds[bindProgramIndex].args);
            auto program = bindProgramArgs.program;
            VKDynamicBindResourceCmd* dynamicBindResourceCmd = &std::get<VKDynamicBindResourceCmd>(cmds[bindSetCmdIndex].args);
            uint32_t updateSet = dynamicBindResourceCmd->set;
            auto writableResources = GetWritableResourcesNoCache(updateSet, *dynamicBindResourceCmd, program, this);
            MakeBarrierFromWritableResources(shaderImageSampleIgnoreList, barrierCountAdded, writableResources);
        }
    }
}

void VKCommandBufferProcessor::FlushAllBindedSetUpdate(
    int inflightIndex,
    std::vector<VKCmd>& cmds,
    std::vector<VKImage*>& shaderImageSampleIgnoreList,
    int& barrierCountAdded
)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: FlushAllBindedSetUpdate");

    for (int i = 0; i < 4; ++i)
    {
        if (recordState.bindedSetUpdateNeeded[i] == true)
        {
            recordState.bindedSetUpdateNeeded[i] = false;
            auto bindSetCmdIndex = recordState.bindSetCmdIndex[i];
            auto bindProgramIndex = recordState.bindProgramIndex;
            auto& bindProgramArgs = std::get<VKBindShaderProgramCmd>(cmds[bindProgramIndex].args);
            auto program = bindProgramArgs.program;
            VKBindResourceCmd* bindSetCmd = &std::get<VKBindResourceCmd>(cmds[bindSetCmdIndex].args);
            uint32_t updateSet = bindSetCmd->set;
            VKShaderResource* resource = bindSetCmd->resource;
            if (resource == nullptr)
                continue;
            auto& writableResources = resource->GetWritableResources(
                inflightIndex,
                updateSet,
                program,
                this
            );
            MakeBarrierFromWritableResources(shaderImageSampleIgnoreList, barrierCountAdded, writableResources);
        }
    }
}

Gfx::VKImage* ImageIdentifier_GetImage(const Gfx::ImageIdentifier& id, Gfx::VKCommandBufferProcessor* graph)
{
    auto idType = id.GetType();
    if (idType == ImageIdentifier::Type::Image)
    {
        auto image = id.GetAsImage();
        return static_cast<Gfx::VKImage*>(image);
    }
    else if (idType == ImageIdentifier::Type::ImageView)
    {
        auto imageView = id.GetAsImageView();
        return static_cast<Gfx::VKImage*>(&imageView->GetImage());
    }
    else if (idType == ImageIdentifier::Type::Handle && graph != nullptr)
    {
        return graph->GetImage(id.GetAsUUID());
    }

    return nullptr;
}

Gfx::VKImageView* ImageIdentifier_GetImageView(const Gfx::ImageIdentifier& id, Gfx::VKCommandBufferProcessor* graph)
{
    auto idType = id.GetType();
    if (idType == ImageIdentifier::Type::Image)
    {
        auto image = id.GetAsImage();
        return static_cast<Gfx::VKImageView*>(&image->GetDefaultImageView());
    }
    else if (idType == ImageIdentifier::Type::ImageView)
    {
        auto imageView = id.GetAsImageView();
        return static_cast<Gfx::VKImageView*>(imageView);
    }
    else if (idType == ImageIdentifier::Type::Handle && graph != nullptr)
    {
        return static_cast<Gfx::VKImageView*>(&graph->GetImage(id.GetAsUUID())->GetDefaultImageView());
    }

    return nullptr;
}

void VKCommandBufferProcessor::BeginRenderPass(
    VkCommandBuffer vkcmd,
    VKRenderPass* renderPass,
    VkClearValue* clearValues,
    int clearValueCount,
    int barrierOffset,
    int barrierCount
)
{
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
    renderPassBeginInfo.clearValueCount = clearValueCount;
    renderPassBeginInfo.pClearValues = clearValues;

    // for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
    // {
    //     PutBarrier(vkcmd, b);
    // }
    PutBarriers(vkcmd, barrierOffset, barrierCount);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {extent.width, extent.height};

    UpdateViewportAndScissorForRenderPass(vkcmd, viewport, scissor, extent);

    vkCmdBeginRenderPass(vkcmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VKCommandBufferProcessor::BindDynamicDescriptorSet(
    VkCommandBuffer cmd, VkPipelineBindPoint bindPoint, VKDynamicBindResourceCmd& dynamicBindResourceCmd, uint32_t set, VKShaderProgram* shaderProgram, int inflightIndex
)
{
    dynamicBindingCache_writes.clear();
    dynamicBindingCache_bufferInfos.clear();
    dynamicBindingCache_imageInfos.clear();
    dynamicBinding_asWrites.clear();
    dynamicBinding_asHandles.clear();

    auto& shaderInfo = shaderProgram->GetShaderInfo();
    std::vector<VKWritableGPUResource> writableGPUResources{};
    auto sharedResource = VKContext::Instance()->sharedResource;
    bool containsTemporaryBuffer = false;

    if (shaderInfo.descriptorSets.size() <= set)
        return;

    const auto& descriptorSet = shaderInfo.descriptorSets[set];
    {
        for (const auto& b : descriptorSet.bindings)
        {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.pNext = VK_NULL_HANDLE;
            w.dstSet = VK_NULL_HANDLE;
            w.descriptorType = MapDescriptorType(b.descriptorType);
            w.dstBinding = b.bindingNum;
            w.dstArrayElement = 0;
            w.descriptorCount = b.descriptorCount;
            w.pImageInfo = VK_NULL_HANDLE;
            w.pBufferInfo = VK_NULL_HANDLE;
            w.pTexelBufferView = VK_NULL_HANDLE;

            auto& bindings = dynamicBindResourceCmd.bindings;
            auto binding = std::ranges::find_if(
                bindings,
                [&b](DynamicBinding& binding)
                { return binding.name == b.name; }
            );

            // store index as fake pointer; patched to real pointer before RequestDescriptorSet
            switch (b.descriptorType)
            {
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer:
                    w.pBufferInfo = (VkDescriptorBufferInfo*)(uintptr_t)dynamicBindingCache_bufferInfos.size();
                    break;
                case DescriptorType::CombinedImageSampler:
                case DescriptorType::StorageImage:
                case DescriptorType::SampledImage:
                case DescriptorType::Sampler:
                    w.pImageInfo = (VkDescriptorImageInfo*)(uintptr_t)dynamicBindingCache_imageInfos.size();
                    break;
                case DescriptorType::AccelerationStructure:
                    {
                        VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
                        asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                        asWrite.pNext = VK_NULL_HANDLE;
                        asWrite.accelerationStructureCount = b.descriptorCount;
                        asWrite.pAccelerationStructures = (VkAccelerationStructureKHR*)(uintptr_t)dynamicBinding_asHandles.size();
                        dynamicBinding_asWrites.push_back(asWrite);
                        w.pNext = (void*)(uintptr_t)(dynamicBinding_asWrites.size() - 1);
                    }
                    break;
                default: break;
            }

            dynamicBindingCache_writes.push_back(w);

            VKImageView* imageView = nullptr;
            VKBuffer* buffer = nullptr;
            AccelerationStructureRef asRef;

            if (binding != bindings.end())
            {
                GetImageViewOrBufferOrAccelerationStructure(*binding, imageView, buffer, asRef);
            }

            for (int i = 0; i < (int)dynamicBindingCache_writes.back().descriptorCount; ++i)
            {
                switch (b.descriptorType)
                {
                    case DescriptorType::UniformBuffer:
                    case DescriptorType::StorageBuffer:
                        {
                            dynamicBindingCache_bufferInfos.push_back({});
                            VkDescriptorBufferInfo& bufferInfo = dynamicBindingCache_bufferInfos.back();
                            VKBuffer* bufferUsed = nullptr;

                            if (binding != bindings.end() && binding->bufferIdentifier.IsTemporary())
                            {
                                auto resolved = ResolveBuffer(binding->bufferIdentifier, inflightIndex);
                                if (resolved.buffer != VK_NULL_HANDLE)
                                {
                                    bufferInfo.buffer = resolved.buffer;
                                    bufferInfo.offset = resolved.offset;
                                    bufferInfo.range = resolved.size;
                                    containsTemporaryBuffer = true;
                                    break;
                                }
                            }

                            if (buffer == nullptr)
                            {
                                if (b.descriptorType == DescriptorType::UniformBuffer)
                                    bufferUsed = defaultUBOBuffer.get();
                                else if (b.descriptorType == DescriptorType::StorageBuffer)
                                    bufferUsed = defaultSSBOBuffer.get();
                            }
                            else
                            {
                                bufferUsed = buffer;
                            }

                            bufferInfo.buffer = bufferUsed->GetHandle();
                            bufferInfo.offset = 0;
                            bufferInfo.range = VK_WHOLE_SIZE;
                            break;
                        }
                    case DescriptorType::StorageImage:
                        {
                            // it's possible a storage image isn't used if it's an array
                            if (imageView == nullptr)
                            {
                                if (b.isTextureArray)
                                {
                                    auto& imageView = sharedResource->GetDefaultStoargeImage2D()->GetImageView(Gfx::ImageViewOption{0, 1, 0, 1, Gfx::ImageAspect::Color, true});
                                    dynamicBindingCache_imageInfos.push_back({});
                                    VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                                    imageInfo.sampler = VK_NULL_HANDLE;
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    imageInfo.imageView = static_cast<VKImageView&>(imageView).GetHandle();
                                }
                                else
                                {
                                    dynamicBindingCache_imageInfos.push_back({});
                                    VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                                    imageInfo.sampler = VK_NULL_HANDLE;
                                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                    imageInfo.imageView =
                                        sharedResource->GetDefaultStoargeImage2D()->GetDefaultVkImageView();
                                }
                            }
                            else
                            {
                                VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);

                                VKWritableGPUResource gpuResource{
                                    .type = VKWritableGPUResource::Type::Image,
                                    .data = ObjPtr<Image>(&imageView->GetImage()),
                                    .stages = pipelineStages,
                                    .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                    .imageView = imageView,
                                    .layout = VK_IMAGE_LAYOUT_GENERAL,
                                };

                                writableGPUResources.push_back(gpuResource);

                                dynamicBindingCache_imageInfos.push_back({});
                                VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                                imageInfo.sampler = sharedResource->GetDefaultSampler();
                                if (imageView != nullptr)
                                {
                                    imageInfo.imageView = imageView->GetHandle();
                                }
                                else
                                {
                                    // using ImageID is not supported in shader resource because we don't have the chance to know if the underlying image is changed in shader resource
                                    throw std::runtime_error("a storage image has to be set before use");
                                    // imageInfo.imageView =
                                    // sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                }
                            }

                            break;
                        }
                    case DescriptorType::CombinedImageSampler:
                    case DescriptorType::SampledImage:
                        {
                            if (b.textureType == TextureType::Tex2D || b.textureType == TextureType::Tex3D)
                            {
                                dynamicBindingCache_imageInfos.push_back({});
                                VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = b.descriptorType == DescriptorType::SampledImage
                                                        ? sharedResource->GetDefaultSampler()
                                                        : VK_NULL_HANDLE;
                                if (imageView != nullptr)
                                {
                                    imageInfo.imageView = imageView->GetHandle();
                                }
                                else
                                {
                                    if (b.textureType == TextureType::Tex2D)
                                        imageInfo.imageView =
                                            sharedResource->GetDefaultTexture2D()->GetDefaultVkImageView();
                                    else if (b.textureType == TextureType::Tex3D)
                                        imageInfo.imageView =
                                            sharedResource->GetDefaultTexture3D()->GetDefaultVkImageView();
                                }
                            }
                            else if (b.textureType == TextureType::TexCube)
                            {
                                dynamicBindingCache_imageInfos.push_back({});
                                VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                imageInfo.sampler = sharedResource->GetDefaultSampler();

                                if (imageView != nullptr && imageView->GetImage().GetDescription().isCubemap)
                                {
                                    imageInfo.imageView = imageView->GetHandle();
                                }
                                else
                                {
                                    imageInfo.imageView =
                                        sharedResource->GetDefaultTextureCube()->GetDefaultVkImageView();
                                }
                            }

                            if (imageView && imageView->GetImage().IsGPUWrite())
                            {
                                VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);
                                VKWritableGPUResource gpuResource{
                                    .type = VKWritableGPUResource::Type::Image,
                                    .data = ObjPtr<Image>(&imageView->GetImage()),
                                    .stages = pipelineStages,
                                    .access = VK_ACCESS_SHADER_READ_BIT,
                                    .imageView = imageView,
                                    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                };

                                writableGPUResources.push_back(gpuResource);
                            }
                            break;
                        }
                    case DescriptorType::Sampler:
                        {
                            auto createInfo = SamplerCachePool::GenerateSamplerCreateInfo(
                                descriptorSet.samplerConfigs[b.samplerIndex]
                            );
                            VkSampler sampler = SamplerCachePool::RequestSampler(createInfo);
                            dynamicBindingCache_imageInfos.push_back({});
                            VkDescriptorImageInfo& imageInfo = dynamicBindingCache_imageInfos.back();
                            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            imageInfo.sampler = sampler;
                            imageInfo.imageView = VK_NULL_HANDLE;
                            break;
                        }
                    case DescriptorType::AccelerationStructure:
                        {
                            if (asRef.context != nullptr)
                            {
                                auto rayTracingContext = static_cast<VKRayTracingContext*>(asRef.context);
                                dynamicBinding_asHandles.push_back((VkAccelerationStructureKHR)rayTracingContext->GetNativeHandle(asRef.scene));
                            }
                            else
                            {
                                dynamicBinding_asHandles.push_back(VK_NULL_HANDLE);
                            }
                            break;
                        }
                    default: ASSERT(0 && "Not implemented"); break;
                }
            }
        }

        // patch fake-pointer indices into real pointers now that vectors are stable
        for (auto& asw : dynamicBinding_asWrites)
            asw.pAccelerationStructures = dynamicBinding_asHandles.data() + (uintptr_t)asw.pAccelerationStructures;
        for (auto& w : dynamicBindingCache_writes)
        {
            switch (w.descriptorType)
            {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    w.pBufferInfo = dynamicBindingCache_bufferInfos.data() + (uintptr_t)w.pBufferInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                case VK_DESCRIPTOR_TYPE_SAMPLER:
                    w.pImageInfo = dynamicBindingCache_imageInfos.data() + (uintptr_t)w.pImageInfo;
                    break;
                case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                    w.pNext = dynamicBinding_asWrites.data() + (uintptr_t)w.pNext;
                    break;
                default: break;
            }
        }

        VkDescriptorSet finalSet = containsTemporaryBuffer
                                       ? RequestTransientDescriptorSet(dynamicBindingCache_writes, set, shaderProgram, inflightIndex)
                                       : RequestDescriptorSet(dynamicBindingCache_writes, set, shaderProgram);
        vkCmdBindDescriptorSets(
            cmd,
            bindPoint,
            exeState.bindedShader->GetVKPipelineLayout(),
            set,
            1,
            &finalSet,
            0,
            VK_NULL_HANDLE
        );
        exeState.bindedDescriptorSets[set] = finalSet;
    }
}

void VKCommandBufferProcessor::GetImageViewOrBufferOrAccelerationStructure(DynamicBinding& binding, VKImageView*& imageView, VKBuffer*& buffer, AccelerationStructureRef& asRef)
{
    if (binding.bufferIdentifier.type == BufferIdentifier::Type::RawBuffer)
    {
        buffer = static_cast<VKBuffer*>(binding.bufferIdentifier.buffer);
    }
    else if (binding.asRef.context != nullptr)
    {
        asRef = binding.asRef;
    }
    else
    {
        if (binding.imageIdentifier.GetType() == ImageIdentifier::Type::Image)
        {
            imageView = static_cast<VKImageView*>(&binding.imageIdentifier.GetAsImage()->GetDefaultImageViewForShaderResource());
        }
        else if (binding.imageIdentifier.GetType() == ImageIdentifier::Type::ImageView)
            imageView = static_cast<VKImageView*>(binding.imageIdentifier.GetAsImageView());
        else if (binding.imageIdentifier.GetType() == ImageIdentifier::Type::Handle)
        {
            auto image = GetImage(binding.imageIdentifier.GetAsUUID());
            if (image)
                imageView = static_cast<VKImageView*>(&image->GetDefaultImageViewForShaderResource());
        }
    }
}
std::vector<VKWritableGPUResource> VKCommandBufferProcessor::GetWritableResourcesNoCache(uint32_t set, VKDynamicBindResourceCmd& dynamicBindResourceCmd, VKShaderProgram* shaderProgram, VKCommandBufferProcessor* graph)
{
    auto& shaderInfo = shaderProgram->GetShaderInfo();
    std::vector<VKWritableGPUResource> writableGPUResources{};

    if (shaderInfo.descriptorSets.size() <= set)
        return {};

    const auto& descriptorSet = shaderInfo.descriptorSets[set];
    auto& bindings = dynamicBindResourceCmd.bindings;
    for (const auto& b : descriptorSet.bindings)
    {
        ShaderBindingHandle bindingHandle(b.name);

        auto binding = std::ranges::find_if(
            bindings,
            [&b](DynamicBinding& binding)
            { return binding.name == b.name; }
        );

        VKImageView* imageView = nullptr;
        VKBuffer* buffer = nullptr;
        AccelerationStructureRef asRef;

        if (binding != bindings.end())
        {
            GetImageViewOrBufferOrAccelerationStructure(*binding, imageView, buffer, asRef);
        }

        for (int i = 0; i < b.descriptorCount; ++i)
        {
            switch (b.descriptorType)
            {
                case DescriptorType::UniformBuffer:
                case DescriptorType::StorageBuffer:
                    {
                        VKBuffer* bufferUsed = nullptr;
                        if (buffer == nullptr)
                        {
                            if (b.descriptorType == DescriptorType::UniformBuffer)
                                bufferUsed = defaultUBOBuffer.get();
                            else if (b.descriptorType == DescriptorType::StorageBuffer)
                                bufferUsed = defaultSSBOBuffer.get();
                        }
                        else
                        {
                            bufferUsed = buffer;
                        }

                        if (bufferUsed->IsGPUWrite())
                        {
                            VkPipelineStageFlags pipelineStages = 0;
                            if (HasFlag(b.stages, ShaderStage::Vertex))
                                pipelineStages |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                            if (HasFlag(b.stages, ShaderStage::Fragment))
                                pipelineStages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                            if (HasFlag(b.stages, ShaderStage::Compute))
                                pipelineStages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

                            VKWritableGPUResource gpuResource{
                                .type = VKWritableGPUResource::Type::Buffer,
                                .data = ObjPtr<Buffer>(bufferUsed),
                                .stages = pipelineStages,
                                .access = static_cast<VkAccessFlags>(
                                    VK_ACCESS_SHADER_READ_BIT |
                                    (b.descriptorType == DescriptorType::StorageBuffer
                                         ? VK_ACCESS_SHADER_WRITE_BIT
                                         : 0)
                                ),
                            };

                            writableGPUResources.push_back(gpuResource);
                        }
                        break;
                    }
                case DescriptorType::StorageImage:
                    {
                        if (imageView != nullptr)
                        {
                            VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);

                            VKWritableGPUResource gpuResource{
                                .type = VKWritableGPUResource::Type::Image,
                                .data = ObjPtr<Image>(&imageView->GetImage()),
                                .stages = pipelineStages,
                                .access = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                .imageView = imageView,
                                .layout = VK_IMAGE_LAYOUT_GENERAL,
                            };

                            writableGPUResources.push_back(gpuResource);
                        }

                        break;
                    }
                case DescriptorType::CombinedImageSampler:
                case DescriptorType::SampledImage:
                    {
                        if (imageView && imageView->GetImage().IsGPUWrite())
                        {
                            VkPipelineStageFlags pipelineStages = ShaderStageToPipelineStage(b.stages);
                            VKWritableGPUResource gpuResource{
                                .type = VKWritableGPUResource::Type::Image,
                                .data = ObjPtr<Image>(&imageView->GetImage()),
                                .stages = pipelineStages,
                                .access = VK_ACCESS_SHADER_READ_BIT,
                                .imageView = imageView,
                                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                            };

                            writableGPUResources.push_back(gpuResource);
                        }
                        break;
                    }
                case DescriptorType::Sampler:
                case DescriptorType::AccelerationStructure:
                    {
                        break;
                    }
                default: ASSERT(0 && "Not implemented"); break;
            }
        }
    }

    return writableGPUResources;
}

VkDescriptorSet VKCommandBufferProcessor::RequestDescriptorSet(std::span<VkWriteDescriptorSet> writes, uint32_t set, VKShaderProgram* shaderProgram)
{
    uint64_t hash = 0;
    int writeCount = writes.size();
    for (int writeIndex = 0; writeIndex < writeCount; ++writeIndex)
    {
        Hash64(hash, writes[writeIndex].descriptorCount);
        if (writes[writeIndex].pImageInfo != nullptr)
        {
            for (int i = 0; i < writes[writeIndex].descriptorCount; ++i)
            {
                Hash64(hash, *writes[writeIndex].pImageInfo);
            }
        }
        else if (writes[writeIndex].pBufferInfo != nullptr)
        {
            for (int i = 0; i < writes[writeIndex].descriptorCount; ++i)
            {
                Hash64(hash, *writes[writeIndex].pBufferInfo);
            }
        }
    }
    auto iter = descriptorSetCache.find(hash);
    VkDescriptorSet finalSet = VK_NULL_HANDLE;
    if (iter == descriptorSetCache.end())
    {
        finalSet = shaderProgram->GetDescriptorPool(set)->Allocate();
        for (int writeIndex = 0; writeIndex < writeCount; ++writeIndex)
        {
            writes[writeIndex].dstSet = finalSet;
        }
        vkUpdateDescriptorSets(GetDevice(), writeCount, writes.data(), 0, VK_NULL_HANDLE);
        descriptorSetCache[hash] = DescriptorSetCacheInfo{finalSet, set, shaderProgram};
    }
    else
    {
        finalSet = iter->second.set;
    }

    return finalSet;
}

VKResolvedBuffer VKCommandBufferProcessor::ResolveBuffer(BufferIdentifier identifier, int inflightIndex)
{
    VKResolvedBuffer resolved{};

    if (identifier.type == BufferIdentifier::Type::RawBuffer)
    {
        auto* vkBuf = static_cast<VKBuffer*>(identifier.buffer);
        if (!vkBuf)
        {
            SPDLOG_ERROR("VKCommandBufferProcessor::ResolveBuffer: RawBuffer identifier has null buffer");
            return resolved;
        }
        resolved.buffer = vkBuf->GetHandle();
        resolved.deviceAddress = vkBuf->GetVKShaderDeviceAddress();
        resolved.mappedData = vkBuf->GetCPUVisibleAddress();
        resolved.offset = 0;
        resolved.size = vkBuf->GetSize();
    }
    else if (identifier.type == BufferIdentifier::Type::TemporaryBuffer)
    {
        auto& map = temporaryBuffers[inflightIndex];
        auto it = map.find(identifier.temporaryBuffer.id);
        if (it != map.end())
        {
            auto& tmp = it->second;
            resolved.buffer = tmp.buffer;
            resolved.deviceAddress = tmp.deviceAddress;
            resolved.mappedData = tmp.mappedData;
            resolved.offset = tmp.offset;
            resolved.size = tmp.size;
        }
        else
        {
            SPDLOG_ERROR("VKCommandBufferProcessor::ResolveBuffer: TemporaryBuffer identifier not found for id={}", identifier.temporaryBuffer.id);
            return resolved;
        }
    }
    else if (identifier.type == BufferIdentifier::Type::None)
    {
        return resolved;
    }

    return resolved;
}

VKCommandBufferProcessor::VKBufferResourceRef VKCommandBufferProcessor::ResolveTrackableBuffer(BufferIdentifier identifier, int inflightIndex)
{
    VKBufferResourceRef ref{};

    if (identifier.type == BufferIdentifier::Type::RawBuffer)
    {
        auto* vkBuf = static_cast<VKBuffer*>(identifier.buffer);
        if (vkBuf)
        {
            ref.trackingId = static_cast<uint64_t>(std::hash<UUID>{}(vkBuf->GetUUID()));
            ref.buffer = vkBuf->GetHandle();
            ref.offset = 0;
            ref.size = VK_WHOLE_SIZE;
            ref.rawBuffer = vkBuf;
        }
    }
    else if (identifier.type == BufferIdentifier::Type::TemporaryBuffer)
    {
        auto& map = temporaryBuffers[inflightIndex];
        auto it = map.find(identifier.temporaryBuffer.id);
        if (it != map.end())
        {
            auto& tmp = it->second;
            ref.trackingId = identifier.temporaryBuffer.id | (static_cast<uint64_t>(1) << 63);
            ref.buffer = tmp.buffer;
            ref.offset = tmp.offset;
            ref.size = tmp.size;
        }
    }

    return ref;
}

void VKCommandBufferProcessor::FrameFinished(int inflightIndex)
{
    temporaryBuffers[inflightIndex].clear();
    if (static_cast<int>(transientDescriptorSets.size()) > inflightIndex)
    {
        for (auto& descriptorSet : transientDescriptorSets[inflightIndex])
        {
            if (descriptorSet.pool != nullptr && descriptorSet.set != VK_NULL_HANDLE)
                descriptorSet.pool->Deallocate(descriptorSet.set);
        }
        transientDescriptorSets[inflightIndex].clear();
    }
}

VkDescriptorSet VKCommandBufferProcessor::RequestTransientDescriptorSet(
    std::span<VkWriteDescriptorSet> writes, uint32_t set, VKShaderProgram* shaderProgram, int inflightIndex
)
{
    auto* pool = shaderProgram->GetDescriptorPool(set);
    auto setHandle = pool->Allocate();

    for (auto& w : writes)
        w.dstSet = setHandle;

    vkUpdateDescriptorSets(GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, VK_NULL_HANDLE);

    if (static_cast<int>(transientDescriptorSets.size()) > inflightIndex)
        transientDescriptorSets[inflightIndex].push_back({pool, setHandle});

    return setHandle;
}

} // namespace Gfx
