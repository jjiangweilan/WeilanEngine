#include "VKCommandBufferProcessor.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Libs/Assert.hpp"
#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKDriver.hpp"
#include "VKExtensionFunc.hpp"
#include "VKShaderProgram.hpp"
#include "VKShaderResource.hpp"
#include "VKUtils.hpp"

namespace Gfx
{
class VKCommandBufferProcessor::ResourceAllocator
{
public:
    ResourceAllocator(VKCommandBufferProcessor* graph) : graph(graph) {}
    VKImage* GetImage(const UUID& hash)
    {
        auto iter = images.find(hash);
        if (iter != images.end())
        {
            return iter->second.image.get();
        }
        return nullptr;
    }

    VKImage* Request(const ImageIdentifier& id, RenderImageDescriptor& desc)
    {
        auto iter = images.find(id.GetAsUUID());
        if (iter != images.end() && iter->second.desc == desc)
        {
            iter->second.frameCountFromLastRequest = 0;
            return iter->second.image.get();
        }
        else
        {
            if (iter != images.end())
            {
                RemoveImageRelatedInfo(iter->second.image.get());
                images.erase(iter);
            }

            // id = ImageIdentifier();
            Gfx::ImageDescription imageDesc;
            imageDesc.width = desc.GetWidth();
            imageDesc.height = desc.GetHeight();
            imageDesc.depth = 1;
            imageDesc.format = desc.GetFormat();
            imageDesc.multiSampling = MultiSampling::Sample_Count_1;
            imageDesc.mipLevels = 1;
            imageDesc.isCubemap = false;

            images[id.GetAsUUID()] = {
                std::make_unique<VKImage>(
                    imageDesc,
                    (Gfx::IsColoFormat(imageDesc.format) ? Gfx::ImageUsage::ColorAttachment
                                                         : Gfx::ImageUsage::DepthStencilAttachment) |
                        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture |
                        (desc.GetRandomWrite() ? Gfx::ImageUsage::Storage : 0)
                ),
                0,
                desc
            };

            auto& image = images[id.GetAsUUID()].image;
            image->SetName(
                fmt::format(
                    "rg-{}-{}",
                    id.GetName().empty() ? id.GetAsUUID().ToString() : id.GetName(),
                    reinterpret_cast<size_t>(image->GetImage())
                )
            );
            SPDLOG_TRACE(
                "VKCommandBufferProcessor: create new iamge({}) {}",
                reinterpret_cast<size_t>(image.get()),
                id.GetAsUUID().ToString()
            );
            return image.get();
        }
    }

    VKRenderPass* Request(RenderPass& renderPass)
    {
        auto iter = renderPasses.find(renderPass);
        if (iter != renderPasses.end() && iter->second.CheckValidationOfAttachments())
        {
            iter->second.frameCountFromLastRequest = 0;
            return iter->second.renderPass.get();
        }
        else
        {
            auto renderPassObj = std::make_unique<VKRenderPass>();
            std::vector<ObjPtr<Image>> imageReferences;
            std::vector<ObjPtr<ImageView>> imageViewReferences;

            auto attachments = renderPass.GetAttachments();
            for (auto& subpass : renderPass.GetSubpasses())
            {
                std::vector<Attachment> colors;
                for (const SubpassAttachment& color : subpass.colors)
                {
                    const auto& id = attachments[color.attachmentIndex];
                    auto idType = id.GetType();
                    Gfx::VKImage* image = ImageIdentifier_GetImage(id, graph);
                    Gfx::VKImageView* imageView = idType == ImageIdentifier::Type::ImageView
                                                      ? static_cast<Gfx::VKImageView*>(id.GetAsImageView())
                                                      : static_cast<Gfx::VKImageView*>(&image->GetDefaultImageView());
                    imageReferences.push_back(image);

                    if (idType == ImageIdentifier::Type::ImageView)
                    {
                        imageViewReferences.push_back(imageView);
                    }

                    colors.push_back(
                        Attachment{
                            imageView,
                            Gfx::MultiSampling::Sample_Count_1,
                            color.loadOp,
                            color.storeOp,
                            color.stencilLoadOp,
                            color.stencilStoreOp,
                        }
                    );
                }

                std::optional<Attachment> depth;
                if (subpass.depth.attachmentIndex != -1)
                {
                    const auto& id = attachments[subpass.depth.attachmentIndex];
                    auto idType = id.GetType();
                    Gfx::VKImage* image = ImageIdentifier_GetImage(id, graph);
                    Gfx::VKImageView* imageView = idType == ImageIdentifier::Type::ImageView
                                                      ? static_cast<Gfx::VKImageView*>(id.GetAsImageView())
                                                      : static_cast<Gfx::VKImageView*>(&image->GetDefaultImageView());
                    imageReferences.push_back(image);

                    if (idType == ImageIdentifier::Type::ImageView)
                    {
                        imageViewReferences.push_back(imageView);
                    }

                    depth = Attachment{
                        imageView,
                        Gfx::MultiSampling::Sample_Count_1,
                        subpass.depth.loadOp,
                        subpass.depth.storeOp,
                        subpass.depth.stencilLoadOp,
                        subpass.depth.stencilStoreOp,
                    };
                }

                renderPassObj->AddSubpass(colors, depth);
            }

            auto temp = renderPassObj.get();
            SPDLOG_TRACE(
                "VKCommandBufferProcessor: create render pass({}) {}",
                reinterpret_cast<size_t>(temp),
                renderPass.GetName()
            );
            renderPasses[renderPass] =
                {std::move(renderPassObj), std::move(imageReferences), std::move(imageViewReferences), 0};

            return temp;
        }
    }

    void RemoveImageRelatedInfo(Image* ptr)
    {
        for (auto& r : graph->globalResourcePool)
        {
            for (auto& e : r.second)
            {
                if (e.second.type == ResourceType::Image && std::get<ObjPtr<Image>>(e.second.res).Get() == ptr)
                {
                    e.second.res = ObjPtr<Image>(nullptr);
                }
            }
        }

        graph->resourceUsageTracks.erase(ptr->GetUUID());
    }

    void Tick()
    {
        // remove images
        int removeCount = 0;
        UUID readyToRemove[8];
        for (auto& iter : images)
        {
            if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
            {
                readyToRemove[removeCount++] = iter.first;
                RemoveImageRelatedInfo(iter.second.image.get());
            }
            iter.second.frameCountFromLastRequest += 1;
        }

        for (int i = 0; i < removeCount; ++i)
        {
            images.erase(readyToRemove[i]);
        }

        UpdateResources(renderPasses);
    }

private:
    int maxResourceUnusedFrames = 120;
    struct AllocatedImage
    {
        std::unique_ptr<VKImage> image;
        int frameCountFromLastRequest = 0;
        RenderImageDescriptor desc;
    };

    struct AllocatedRenderPass
    {
        std::unique_ptr<VKRenderPass> renderPass;
        std::vector<ObjPtr<Image>> attachments;
        std::vector<ObjPtr<ImageView>> imageViews;
        int frameCountFromLastRequest = 0;

        bool CheckValidationOfAttachments()
        {
            for (auto& r : attachments)
            {
                if (r == nullptr)
                    return false;
            }

            for (auto& v : imageViews)
            {
                if (v == nullptr)
                    return false;
            }

            return true;
        }
    };

    VKCommandBufferProcessor* graph;
    std::unordered_map<UUID, AllocatedImage> images;
    std::unordered_map<RenderPass, AllocatedRenderPass> renderPasses;

    template <class T>
    void UpdateResources(std::unordered_map<UUID, T>& resources)
    {
        int removeCount = 0;
        const UUID* readyToRemove[8];
        for (auto& iter : resources)
        {
            if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
            {
                readyToRemove[removeCount++] = &iter.first;
            }
            iter.second.frameCountFromLastRequest += 1;
        }

        for (int i = 0; i < removeCount; ++i)
        {
            resources.erase(*readyToRemove[i]);
        }
    }

    void UpdateResources(std::unordered_map<RenderPass, AllocatedRenderPass>& resources)
    {
        int removeCount = 0;
        const RenderPass* readyToRemove[8];
        for (auto& iter : resources)
        {
            if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
            {
                readyToRemove[removeCount++] = &iter.first;
            }
            iter.second.frameCountFromLastRequest += 1;
        }

        for (int i = 0; i < removeCount; ++i)
        {
            resources.erase(*readyToRemove[i]);
        }
    }
};

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
    return resourceAllocator->Request(id, desc);
}

VKRenderPass* VKCommandBufferProcessor::Request(RenderPass& renderPass)
{
    return resourceAllocator->Request(renderPass);
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

void VKCommandBufferProcessor::GoThroughRenderPass(
    std::vector<VKCmd>& exectedCmds,
    VKRenderPass& renderPass,
    int& visitIndex,
    int& barrierCountResult,
    int& barrierOffsetResult
)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - GoThroughRenderPass");
    int barrierOffset = barriers.size();
    int barrierCount = 0;

    // handle case like shadow map being binded to global descriptor set but also set to render pass attachment
    std::vector<VKImage*> shaderImageSampleIgnoreList;
    shaderImageSampleIgnoreList.reserve(8);

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
                barrierCount += MakeBarrierForLastUsage(image, image->GetUUID());
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
                barrierCount += MakeBarrierForLastUsage(image, image->GetUUID());
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
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            ScheduleBindShaderProgram(cmd, visitIndex);
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed ||
                 cmd.type == VKCmdType::DrawIndirect || cmd.type == VKCmdType::DrawIndexedIndirect)
        {
            FlushAllBindedSetUpdate(exectedCmds, shaderImageSampleIgnoreList, barrierCount);
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
                {ResourceType::Buffer, ObjPtr<Buffer>(args.buffer), std::nullopt};
        }
        else if (visitIndex >= exectedCmds.size())
            break;
    }

    barrierCountResult = barrierCount;
    barrierOffsetResult = barrierOffset;
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
                        image->SetLayout(subresourceRange, currentUsage.layout);
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
                    image->SetLayout(vkRange, currentUsage.layout);
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
            image->SetLayout(subresourceRange, currentUsage.layout);
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

void VKCommandBufferProcessor::FlushBindResourceTrack() {}

size_t VKCommandBufferProcessor::TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor - TrackResourceForPushDescriptorSet");
    int imageIndex = 0;
    int barrierCount = 0;
    auto& pushDescriptorCmd = std::get<VKPushDescriptorCmd>(cmd.args);
    for (int bindingIndex = 0; bindingIndex < pushDescriptorCmd.bindingCount; ++bindingIndex)
    {
        auto& b = pushDescriptorCmd.bindings[bindingIndex];
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
                        barrierCount += MakeBarrierForLastUsage(image, image->GetUUID());
                    }
                }
            }
        }
    }

    return barrierCount;
}
void VKCommandBufferProcessor::PreExecute(VKFramePrepareData& framePrepare)
{
    ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor::PreExecute");

    auto& executedCmds = framePrepare.cmds;

    int cmdIndexOffset = executedCmds.size();
    // track where to put barriers
    for (int visitIndex = 0; visitIndex < executedCmds.size(); visitIndex++)
    {
        auto& cmd = executedCmds[visitIndex];
        if (cmd.type == VKCmdType::BeginRenderPass)
        {
            auto& args = std::get<VKBeginRenderPassCmd>(cmd.args);
            GoThroughRenderPass(executedCmds, *args.renderPass, visitIndex, args.barrierCount, args.barrierOffset);
        }
        else if (cmd.type == VKCmdType::AsyncReadback)
        {
            auto& args = std::get<VKAsyncReadbackCmd>(cmd.args);
            asyncReadbacks.push_back(std::move(*args.handle));
        }
        else if (cmd.type == VKCmdType::RGBeginRenderPass)
        {
            auto& args = std::get<VKRGBeginRenderPassCmd>(cmd.args);

            auto renderPass = resourceAllocator->Request(args.renderPass);
            GoThroughRenderPass(executedCmds, *renderPass, visitIndex, args.barrierCount, args.barrierOffset);
        }
        else if (cmd.type == VKCmdType::DynamicBeginRenderPass)
        {
            auto& args = std::get<VKDynamicRenderPassCmd>(cmd.args);
            auto& imgs = args.imageIdentifiers;

            std::optional<SubpassAttachment> depthAttachmentDescription = std::nullopt;
            auto lastImage = ImageIdentifier_GetImage(imgs.back().image, this);
            bool hasDepth = !imgs.empty() && IsDepthStencilFormat(lastImage->GetDescription().format);
            if (hasDepth)
            {
                auto& depthRenderAttachment = *imgs.end();
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
            GoThroughRenderPass(executedCmds, *renderPass, visitIndex, args.barrierCount, args.barrierOffset);
        }
        else if (cmd.type == VKCmdType::BindResource)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: bind resource");
            auto& args = std::get<VKBindResourceCmd>(cmd.args);
            recordState.bindSetCmdIndex[args.set] = visitIndex;
            recordState.bindedSetUpdateNeeded[args.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            ScheduleBindShaderProgram(cmd, visitIndex);
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: copy buffer");
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            auto& args = std::get<VKCopyBufferCmd>(cmd.args);
            if (TrackResource(args.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                barrierCount += MakeBarrierForLastUsage(args.src, args.src->GetUUID());

            if (TrackResource(args.dst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
                barrierCount += MakeBarrierForLastUsage(args.dst, args.dst->GetUUID());

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::CopyBuffer)
        {}
        else if (cmd.type == VKCmdType::Blit)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: blit");
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            auto& args = std::get<VKBlitCmd>(cmd.args);
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
                barrierCount += MakeBarrierForLastUsage(args.from, args.from->GetUUID());

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
                barrierCount += MakeBarrierForLastUsage(args.to, args.to->GetUUID());

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::CopyImageToBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: copy image to buffer");
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            auto& args = std::get<VKCopyImageToBufferCmd>(cmd.args);
            if (TrackResource(args.dst, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT))
                barrierCount += MakeBarrierForLastUsage(args.dst, args.dst->GetUUID());

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
                    barrierCount += MakeBarrierForLastUsage(args.src, args.src->GetUUID());
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
            if (TrackResource(args.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                barrierCount += MakeBarrierForLastUsage(args.src, args.src->GetUUID());

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
                    barrierCount += MakeBarrierForLastUsage(args.dst, args.dst->GetUUID());
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
                barrierCount += MakeBarrierForLastUsage(args.image, args.image->GetUUID());
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
                {ResourceType::Buffer, ObjPtr<Buffer>(args.buffer), std::nullopt};
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
            FlushAllBindedSetUpdate(executedCmds, list, args.barrierCount);
        }
        else if (cmd.type == VKCmdType::DispatchIndirect)
        {
            ENGINE_SCOPED_PROFILE("VKCommandBufferProcessor: dispatchIndir");
            auto& args = std::get<VKDispatchIndirectCmd>(cmd.args);
            std::vector<VKImage*> list;
            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            FlushAllBindedSetUpdate(executedCmds, list, args.barrierCount);
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
    PreExecute(framePrepare);
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

    for (size_t i = 0; i < executedCmds.size(); ++i)
    {
        auto& cmd = executedCmds[i];
        switch (cmd.type)
        {
            case VKCmdType::SetLineWidth:
                {

                    auto& args = std::get<VKSetLineWidthCmd>(cmd.args);
                    vkCmdSetLineWidth(vkcmd, args.lineWidth);
                    break;
                }
            case VKCmdType::DrawIndexed:
                {
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
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
                    auto& args = std::get<VKDrawCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    vkCmdDraw(vkcmd, args.vertexCount, args.instanceCount, args.firstVertex, args.firstInstance);
                    break;
                }
            case Gfx::VKCmdType::ClearColorImage:
                {
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
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
                    vkCmdClearColorImage(vkcmd, vkimage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue, 1, &range);
                    break;
                }
            case VKCmdType::DrawIndirect:
                {
                    auto& args = std::get<VKDrawIndirectCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    vkCmdDrawIndirect(
                        vkcmd,
                        static_cast<VKBuffer*>(args.buffer)->GetHandle(),
                        args.offset,
                        args.drawCount,
                        args.stride
                    );
                    break;
                }
            case VKCmdType::DrawIndexedIndirect:
                {
                    auto& args = std::get<VKDrawIndexedIndirectCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_GRAPHICS);
                    vkCmdDrawIndexedIndirect(
                        vkcmd,
                        static_cast<VKBuffer*>(args.buffer)->GetHandle(),
                        args.offset,
                        args.drawCount,
                        args.stride
                    );
                    break;
                }
            case VKCmdType::AsyncReadback:
                {
                    // vkCmdSetEvent(vkcmd, cmd.asyncReadback.event, VK_PIPELINE_STAGE_TRANSFER_BIT);
                    break;
                }
            case Gfx::VKCmdType::DynamicBeginRenderPass:
                {
                    auto& args = std::get<VKDynamicRenderPassCmd>(cmd.args);
                    VKRenderPass* renderPasss = args.resolvedRenderPass;
                    std::span<ClearValue> clearValues = args.clearValues;

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
                    break;
                }
            case VKCmdType::BeginRenderPass:
                {
                    auto& args = std::get<VKBeginRenderPassCmd>(cmd.args);
                    Gfx::VKRenderPass* renderPass = args.renderPass;
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
                    renderPassBeginInfo.clearValueCount = args.clearValueCount;
                    renderPassBeginInfo.pClearValues = args.clearValues;

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

                    if (!exeState.overrideViewport)
                    {
                        VkViewport viewport;
                        viewport.x = 0.0f;
                        viewport.y = 0.0f;
                        viewport.width = (float)extent.width;
                        viewport.height = (float)extent.height;
                        viewport.minDepth = 0.0f;
                        viewport.maxDepth = 1.0f;
                        vkCmdSetViewport(vkcmd, 0, 1, &viewport);
                        exeState.overrideViewport = false;
                    }

                    if (!exeState.overrideScissor)
                    {
                        VkRect2D scissor;
                        scissor.offset = {0, 0};
                        scissor.extent = {extent.width, extent.height};
                        vkCmdSetScissor(vkcmd, 0, 1, &scissor);
                        exeState.overrideScissor = false;
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
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

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
                    auto& args = std::get<VKBindResourceCmd>(cmd.args);
                    if (args.set > 4)
                        return;

                    exeState.setResources[args.set].resource = (VKShaderResource*)args.resource;
                    exeState.setResources[args.set].needUpdate = true;
                    break;
                }
            case VKCmdType::BindVertexBuffer:
                {
                    auto& args = std::get<VKBindVertexBufferCmd>(cmd.args);
                    VkBuffer vkBuffers[8];
                    uint64_t vkOffsets[8];
                    for (uint32_t i = 0; i < args.vertexBufferBindingCount; ++i)
                    {
                        VKBuffer* vkbuf = static_cast<VKBuffer*>(args.vertexBufferBindings[i].buffer);
                        exeState.vertexBufferBindings[i] = vkbuf;
                        vkBuffers[i] = vkbuf->GetHandle();
                        vkOffsets[i] = args.vertexBufferBindings[i].offset;
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
                    auto& args = std::get<VKBindShaderProgramCmd>(cmd.args);
                    exeState.pendingBindedShader = args.program;
                    exeState.shaderConfig = *args.config;
                    break;
                }
            case VKCmdType::BindIndexBuffer:
                {
                    auto& args = std::get<VKBindIndexBufferCmd>(cmd.args);
                    VKBuffer* buffer = args.buffer;

                    VkBuffer indexBuf = buffer->GetHandle();
                    vkCmdBindIndexBuffer(vkcmd, indexBuf, args.offset, args.indexType);
                    break;
                }
            case VKCmdType::SetViewport:
                {
                    auto& args = std::get<VKSetViewportCmd>(cmd.args);
                    exeState.overrideViewport = true;
                    vkCmdSetViewport(vkcmd, 0, 1, &args.viewport);
                    break;
                }
            case VKCmdType::CopyImageToBuffer:
                {
                    std::vector<VkBufferImageCopy> vkRegions;
                    auto& args = std::get<VKCopyImageToBufferCmd>(cmd.args);

                    for (int i = 0; i < args.regionsCount; ++i)
                    {
                        auto& r = args.regions[i];
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

                        vkRegions.push_back(region);
                    }

                    VKImage* srcImage = args.src;
                    VKBuffer* dstBuffer = args.dst;

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
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
                    auto& args = std::get<VKSetScissorCmd>(cmd.args);
                    exeState.overrideScissor = true;
                    vkCmdSetScissor(vkcmd, args.firstScissor, args.scissorCount, args.rects);
                    break;
                }
            case VKCmdType::Dispatch:
                {
                    auto& args = std::get<VKDispatchCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_COMPUTE);

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

                    VkMemoryBarrier b{
                        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                        VK_NULL_HANDLE,
                        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
                        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT
                    };
                    vkCmdPipelineBarrier(
                        vkcmd,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_DEPENDENCY_BY_REGION_BIT,
                        1,
                        &b,
                        0,
                        VK_NULL_HANDLE,
                        0,
                        VK_NULL_HANDLE
                    );
                    vkCmdDispatch(vkcmd, args.groupCountX, args.groupCountY, args.groupCountZ);
                    break;
                }
            case VKCmdType::DispatchIndirect:
                {
                    auto& args = std::get<VKDispatchIndirectCmd>(cmd.args);
                    TryBindShader(vkcmd);
                    UpdateDescriptorSetBinding(vkcmd, VK_PIPELINE_BIND_POINT_COMPUTE);

                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
                    vkCmdDispatchIndirect(vkcmd, args.buffer->GetHandle(), args.bufferOffset);
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
                        args.shader->GetVKPipelineLayout(),
                        args.set,
                        args.bindingCount,
                        writeSets
                    );

                    break;
                }
            case VKCmdType::CopyBuffer:
                {
                    auto& args = std::get<VKCopyBufferCmd>(cmd.args);
                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;
                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }

                    vkCmdCopyBuffer(
                        vkcmd,
                        args.src->GetHandle(),
                        args.dst->GetHandle(),
                        args.copyRegionCount,
                        args.copyRegions
                    );
                    break;
                }
            case VKCmdType::CopyBufferToImage:
                {
                    auto& args = std::get<VKCopyBufferToImageCmd>(cmd.args);
                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;

                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
                    vkCmdCopyBufferToImage(
                        vkcmd,
                        args.src->GetHandle(),
                        args.dst->GetImage(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        args.regionCount,
                        args.regions
                    );
                    break;
                }
            case VKCmdType::Present:
                {
                    auto& args = std::get<VKPresentCmd>(cmd.args);
                    auto barrierOffset = args.barrierOffset;
                    auto barrierCount = args.barrierCount;

                    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
                    {
                        PutBarrier(vkcmd, b);
                    }
                    break;
                }

            case VKCmdType::RGBeginRenderPass:
                {
                    auto& args = std::get<VKRGBeginRenderPassCmd>(cmd.args);
                    Gfx::VKRenderPass* renderPass = resourceAllocator->Request(args.renderPass);
                    BeginRenderPass(
                        vkcmd,
                        renderPass,
                        args.clearValues,
                        args.clearValueCount,
                        args.barrierOffset,
                        args.barrierCount
                    );
                    break;
                }
            case Gfx::VKCmdType::BeginLabel:
                {
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
                    auto& args = std::get<VKInsertLabelCmd>(cmd.args);
                    VKDebugUtils::CmdInsertLabel(vkcmd, args.label.data(), args.color);
                    break;
                }
            case Gfx::VKCmdType::GraphicsBlit:
                {
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

    resourceAllocator->Tick();
}

void VKCommandBufferProcessor::UpdateDescriptorSetBinding(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint)
{
    UpdateDescriptorSetBinding(cmd, 0, bindPoint);
    UpdateDescriptorSetBinding(cmd, 1, bindPoint);
    UpdateDescriptorSetBinding(cmd, 2, bindPoint);
    UpdateDescriptorSetBinding(cmd, 3, bindPoint);
}

void VKCommandBufferProcessor::TryBindShader(VkCommandBuffer cmd)
{
    if ((exeState.bindedShader != exeState.pendingBindedShader || exeState.shaderConfig != exeState.pendingShaderConfig) &&
        exeState.pendingBindedShader != nullptr)
    {

        if (exeState.pendingBindedShader->IsCompute())
        {
            auto pipeline = exeState.pendingBindedShader->RequestComputePipeline();

            if (pipeline != exeState.lastBindedPipeline)
            {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                exeState.lastBindedPipeline = pipeline;
                exeState.bindedShader = exeState.pendingBindedShader;
                exeState.shaderConfig = exeState.pendingShaderConfig;

                exeState.setResources[0].needUpdate = true;
                exeState.setResources[1].needUpdate = true;
                exeState.setResources[2].needUpdate = true;
                exeState.setResources[3].needUpdate = true;
            }
        }
        else
        {
            // binding pipeline
            ASSERT(exeState.renderPass != nullptr && "RenderPass is null, draw call maybe not inside a RenderPass");

            if (exeState.renderPass != nullptr)
            {
                auto pipeline = exeState.pendingBindedShader->RequestGraphicsPipeline(
                    exeState.shaderConfig,
                    std::span<VKBuffer*>(exeState.vertexBufferBindings, exeState.vertexBufferBindingCount),
                    exeState.renderPass,
                    exeState.subpassIndex
                );

                if (pipeline != exeState.lastBindedPipeline)
                {
                    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    exeState.lastBindedPipeline = pipeline;
                    exeState.bindedShader = exeState.pendingBindedShader;
                    exeState.shaderConfig = exeState.pendingShaderConfig;

                    exeState.setResources[0].needUpdate = true;
                    exeState.setResources[1].needUpdate = true;
                    exeState.setResources[2].needUpdate = true;
                    exeState.setResources[3].needUpdate = true;
                }
            }
            else
                spdlog::error("draw call outside of renderpass");
        }
    }
}

void VKCommandBufferProcessor::UpdateDescriptorSetBinding(
    VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint
)
{
    if (exeState.setResources[index].needUpdate && exeState.setResources[index].resource)
    {
        auto sourceSet =
            exeState.setResources[index].resource->GetDescriptorSet(index, exeState.bindedShader, this);
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

            // if a lower order set is being changed there is high chance that lower order set is being disturbed so we
            // need to bind them again
            for (int i = index + 1; i < 4; ++i)
            {
                exeState.bindedDescriptorSets[i] = VK_NULL_HANDLE;
                exeState.setResources[i].needUpdate = true;
            }
        }
    }
}

void VKCommandBufferProcessor::PutBarrier(VkCommandBuffer vkcmd, int index)
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

void VKCommandBufferProcessor::ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex)
{
    recordState.bindProgramIndex = visitIndex;
}

VKImage* VKCommandBufferProcessor::GetImage(const UUID& hash)
{
    return resourceAllocator->GetImage(hash);
}

VKCommandBufferProcessor::VKCommandBufferProcessor(int inflightCount)
{
    resourceAllocator = std::make_unique<ResourceAllocator>(this);
}
VKCommandBufferProcessor::~VKCommandBufferProcessor() {}

void VKCommandBufferProcessor::FlushAllBindedSetUpdate(
    std::vector<VKCmd>& cmds, std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded
)
{
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
            auto& writableResources = resource->GetWritableResources(updateSet, program, this);
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
                        barrierCountAdded += MakeBarrierForLastUsage(data, data->GetUUID());
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

    for (int b = barrierOffset; b < barrierOffset + barrierCount; ++b)
    {
        PutBarrier(vkcmd, b);
    }

    if (!exeState.overrideViewport)
    {
        VkViewport viewport;
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)extent.width;
        viewport.height = (float)extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(vkcmd, 0, 1, &viewport);
        exeState.overrideViewport = false;
    }

    if (!exeState.overrideScissor)
    {
        VkRect2D scissor;
        scissor.offset = {0, 0};
        scissor.extent = {extent.width, extent.height};
        vkCmdSetScissor(vkcmd, 0, 1, &scissor);
        exeState.overrideScissor = false;
    }

    vkCmdBeginRenderPass(vkcmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

} // namespace Gfx
