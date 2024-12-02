#include "VKRenderGraph.hpp"
#include "../VKBuffer.hpp"
#include "../VKDriver.hpp"
#include "../VKExtensionFunc.hpp"
#include "../VKShaderProgram.hpp"
#include "../VKShaderResource.hpp"
#include "../VKUtils.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Profiler/Profiler.hpp"

namespace Gfx::VK::RenderGraph
{
class Graph::ResourceAllocator
{
public:
    ResourceAllocator(Graph* graph) : graph(graph) {}
    VKImage* GetImage(const UUID& hash)
    {
        auto iter = images.find(hash);
        if (iter != images.end())
        {
            return iter->second.image.get();
        }
        return nullptr;
    }

    VKImage* Request(RG::ImageIdentifier& id, RG::ImageDescription& desc)
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

            // id = RG::ImageIdentifier();
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
            image->SetName(fmt::format(
                "rg-{}-{}",
                id.GetName().empty() ? id.GetAsUUID().ToString() : id.GetName(),
                reinterpret_cast<size_t>(image->GetImage())
            ));
            SPDLOG_INFO(
                "VKRenderGraph: create new iamge({}) {}",
                reinterpret_cast<size_t>(image.get()),
                id.GetAsUUID().ToString()
            );
            return image.get();
        }
    }

    VKRenderPass* Request(RG::RenderPass& renderPass)
    {
        auto uuid = renderPass.GetUUID();
        auto iter = renderPasses.find(uuid);
        if (iter != renderPasses.end() && iter->second.CheckValidationOfAttachments())
        {
            iter->second.frameCountFromLastRequest = 0;
            return iter->second.renderPass.get();
        }
        else
        {
            auto renderPassObj = std::make_unique<VKRenderPass>();
            std::vector<SRef<Image>> imageReferences;

            auto attachments = renderPass.GetAttachments();
            for (auto& subpass : renderPass.GetSubpasses())
            {
                std::vector<Attachment> colors;
                for (RG::SubpassAttachment color : subpass.colors)
                {
                    if (attachments[color.attachmentIndex].GetType() == RG::ImageIdentifier::Type::Image)
                    {
                        auto image = attachments[color.attachmentIndex].GetAsImage();
                        imageReferences.push_back(image->GetSRef());
                        colors.push_back(Attachment{
                            &image->GetDefaultImageView(),
                            Gfx::MultiSampling::Sample_Count_1,
                            color.loadOp,
                            color.storeOp,
                            color.stencilLoadOp,
                            color.stencilStoreOp,
                        });
                    }
                    else if (attachments[color.attachmentIndex].GetType() == RG::ImageIdentifier::Type::Handle)
                    {
                        auto& image = images[attachments[color.attachmentIndex].GetAsUUID()];
                        imageReferences.push_back(image.image->GetSRef());
                        colors.push_back(Attachment{
                            &image.image->GetDefaultImageView(),
                            Gfx::MultiSampling::Sample_Count_1,
                            color.loadOp,
                            color.storeOp,
                            color.stencilLoadOp,
                            color.stencilStoreOp,
                        });
                    }
                }

                std::optional<Attachment> depth;
                if (subpass.depth.attachmentIndex != -1)
                {
                    if (attachments[subpass.depth.attachmentIndex].GetType() == RG::ImageIdentifier::Type::Image)
                    {
                        auto image = attachments[subpass.depth.attachmentIndex].GetAsImage();
                        imageReferences.push_back(image->GetSRef());
                        depth = Attachment{
                            &image->GetDefaultImageView(),
                            Gfx::MultiSampling::Sample_Count_1,
                            subpass.depth.loadOp,
                            subpass.depth.storeOp,
                            subpass.depth.stencilLoadOp,
                            subpass.depth.stencilStoreOp,
                        };
                    }
                    else if (attachments[subpass.depth.attachmentIndex].GetType() == RG::ImageIdentifier::Type::Handle)
                    {
                        auto& image = images[attachments[subpass.depth.attachmentIndex].GetAsUUID()];
                        imageReferences.push_back(image.image->GetSRef());
                        depth = Attachment{
                            &image.image->GetDefaultImageView(),
                            Gfx::MultiSampling::Sample_Count_1,
                            subpass.depth.loadOp,
                            subpass.depth.storeOp,
                            subpass.depth.stencilLoadOp,
                            subpass.depth.stencilStoreOp,
                        };
                    }
                }

                renderPassObj->AddSubpass(colors, depth);
            }

            auto temp = renderPassObj.get();
            SPDLOG_INFO("VKRenderGraph: create render pass({}) {}", reinterpret_cast<size_t>(temp), uuid.ToString());
            renderPasses[uuid] = {std::move(renderPassObj), std::move(imageReferences), 0};

            return temp;
        }
    }

    void RemoveImageRelatedInfo(Image* ptr)
    {
        for (auto& r : graph->globalResourcePool)
        {
            for (auto& e : r.second)
            {
                if (e.second.type == ResourceType::Image && std::get<SRef<Image>>(e.second.res).Get() == ptr)
                {
                    e.second.res = SRef<Image>(nullptr);
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
        RG::ImageDescription desc;
    };

    struct AllocatedRenderPass
    {
        std::unique_ptr<VKRenderPass> renderPass;
        std::vector<SRef<Image>> attachments;
        int frameCountFromLastRequest = 0;

        bool CheckValidationOfAttachments()
        {
            for (auto& r : attachments)
            {
                if (r == nullptr)
                    return false;
            }

            return true;
        }
    };

    Graph* graph;
    std::unordered_map<UUID, AllocatedImage> images;
    std::unordered_map<UUID, AllocatedRenderPass> renderPasses;

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
};

bool Graph::TrackResource(
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
        track.res = writableResource->GetSRef();
        track.currentFrameUsages.push_back({stages, access, range, layout});

        resourceUsageTracks[writableResource->GetUUID()] = track;
        return true;
    }

    return false;
}

VKImage* Graph::Request(RG::ImageIdentifier& id, RG::ImageDescription& desc)
{
    return resourceAllocator->Request(id, desc);
}

VKRenderPass* Graph::Request(RG::RenderPass& renderPass)
{
    return resourceAllocator->Request(renderPass);
}

bool Graph::TrackResource(VKBuffer* writableResource, VkPipelineStageFlags stages, VkAccessFlags access)
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
        track.res = writableResource->GetSRef();
        track.currentFrameUsages.push_back({stages, access, Gfx::ImageSubresourceRange{}, VK_IMAGE_LAYOUT_UNDEFINED});

        resourceUsageTracks[writableResource->GetUUID()] = track;
        return true;
    }

    return false;
}

void Graph::GoThroughRenderPass(
    VKRenderPass& renderPass, int& visitIndex, int& barrierCountResult, int& barrierOffsetResult
)
{
    ENGINE_SCOPED_PROFILE("VKRenderGraph - GoThroughRenderPass");
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
        auto& cmd = currentSchedulingCmds[visitIndex];
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
            FlushAllBindedSetUpdate(shaderImageSampleIgnoreList, barrierCount);
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            barrierCount += TrackResourceForPushDescriptorSet(cmd, true);
        }
        else if (cmd.type == VKCmdType::SetTexture)
        {
            auto& args = std::get<VKSetTextureCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {
                ResourceType::Image, args.image != nullptr ? args.image->GetSRef() : nullptr,
                args.imageViewOption
            };
        }
        else if (cmd.type == VKCmdType::SetBuffer)
        {
            auto& args = std::get<VKSetBufferCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {ResourceType::Buffer, args.buffer->GetSRef(), std::nullopt};
        }
        else if (visitIndex >= currentSchedulingCmds.size())
            break;
    }

    barrierCountResult = barrierCount;
    barrierOffsetResult = barrierOffset;
}

int Graph::MakeBarrierForLastUsage(void* res, const UUID& uuid)
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
        VKImage* image = (VKImage*)std::get<SRef<Image>>(iter->second.res).Get();
        if (image == nullptr)
        {
            // garbage image remove it
            resourceUsageTracks.erase(iter);
            return 0;
        }

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

                    VkImageSubresourceRange subresourceRange = Gfx::MapVkImageSubresourceRange(overlapping);
                    preUsage.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                    if (!image->QueryLayout(subresourceRange, preUsage.layout)) [[unlikely]]
                    {
                        spdlog::error("VKRenderGraph: image layout not properly handled");
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
            remainingRangeSwap.clear();

            // break
            if (remainingRange.empty())
                break;
        }

        // handle the situation when there is not previous usage
        auto subresourceRange = Gfx::MapVkImageSubresourceRange(currentUsage.range);
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (!image->QueryLayout(subresourceRange, layout)) [[unlikely]]
        {
            spdlog::error("VKRenderGraph: image layout not properly handled");
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
        VKBuffer* buffer = ((VKBuffer*)std::get<SRef<Buffer>>(iter->second.res).Get());
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

void Graph::FlushBindResourceTrack() {}

size_t Graph::TrackResourceForPushDescriptorSet(VKCmd& cmd, bool addBarrier)
{
    ENGINE_SCOPED_PROFILE("VKRenderGraph - TrackResourceForPushDescriptorSet");
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
void Graph::Schedule(VKCommandBuffer& cmd)
{
    ENGINE_SCOPED_PROFILE("VKRenderGraph: schedule");

    ENGINE_BEGIN_PROFILE("VKRenderGraph: insert cmds");
    int cmdIndexOffset = currentSchedulingCmds.size();
    currentSchedulingCmds.insert(currentSchedulingCmds.end(), cmd.GetCmds().begin(), cmd.GetCmds().end());
    ENGINE_END_PROFILE

    // track where to put barriers
    for (int visitIndex = cmdIndexOffset; visitIndex < currentSchedulingCmds.size(); visitIndex++)
    {
        auto& cmd = currentSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::BeginRenderPass)
        {
            auto& args = std::get<VKBeginRenderPassCmd>(cmd.args);
            GoThroughRenderPass(
                *args.renderPass,
                visitIndex, args.barrierCount, args.barrierOffset
            );
        }
        else if (cmd.type == VKCmdType::AsyncReadback)
        {
            auto& args = std::get<VKAsyncReadbackCmd>(cmd.args);
            asyncReadbacks.push_back(std::move(*args.handle));
        }
        else if (cmd.type == VKCmdType::RGBeginRenderPass)
        {
            auto& args = std::get<VKRGBeginRenderPassCmd>(cmd.args);
            auto renderPass = resourceAllocator->Request(*args.renderPass);
            GoThroughRenderPass(
                *renderPass,
                visitIndex,
                args.barrierCount,
                args.barrierOffset
            );
        }
        else if (cmd.type == VKCmdType::BindResource)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: bind resource");
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
            ENGINE_SCOPED_PROFILE("VKRenderGraph: copy buffer");
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
        else if (cmd.type == VKCmdType::Blit)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: blit");
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
            ENGINE_SCOPED_PROFILE("VKRenderGraph: copy image to buffer");
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
            ENGINE_SCOPED_PROFILE("VKRenderGraph: copy buffer to image");
            auto& args = std::get<VKCopyBufferToImageCmd>(cmd.args);
            size_t barrierOffset = barriers.size();
            size_t barrierCount = 0;
            if (TrackResource(args.src, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT))
                barrierCount +=
                    MakeBarrierForLastUsage(args.src, args.src->GetUUID());

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
                    barrierCount +=
                        MakeBarrierForLastUsage(args.dst, args.dst->GetUUID());
            }

            args.barrierOffset = barrierOffset;
            args.barrierCount = barrierCount;
        }
        else if (cmd.type == VKCmdType::PushDescriptorSet)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: push descriptor set");
            TrackResourceForPushDescriptorSet(cmd, false);
        }
        else if (cmd.type == VKCmdType::Present)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: present");
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
            ENGINE_SCOPED_PROFILE("VKRenderGraph: set texture");
            auto& args = std::get<VKSetTextureCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] =
                {ResourceType::Image, args.image->GetSRef(), args.imageViewOption};
        }
        else if (cmd.type == VKCmdType::SetBuffer)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: set buffer");
            auto& args = std::get<VKSetBufferCmd>(cmd.args);
            globalResourcePool[args.handle][args.index] = {ResourceType::Buffer, args.buffer->GetSRef(), std::nullopt};
        }
        else if (cmd.type == VKCmdType::AllocateAttachment)
        {
            // done in command buffer
        }
        else if (cmd.type == VKCmdType::Dispatch)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: dispatch");
            std::vector<VKImage*> list;
            auto& args = std::get<VKDispatchCmd>(cmd.args);
            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            FlushAllBindedSetUpdate(list, args.barrierCount);
        }
        else if (cmd.type == VKCmdType::DispatchIndirect)
        {
            ENGINE_SCOPED_PROFILE("VKRenderGraph: dispatchIndir");
            auto& args = std::get<VKDispatchIndirectCmd>(cmd.args);
            std::vector<VKImage*> list;
            args.barrierOffset = barriers.size();
            args.barrierCount = 0;
            FlushAllBindedSetUpdate(list, args.barrierCount);
        }
    }
}

void Graph::Execute(VkCommandBuffer vkcmd)
{
    ENGINE_SCOPED_PROFILE("VKRenderGraph::Execute");
    for (size_t i = 0; i < currentSchedulingCmds.size(); ++i)
    {
        auto& cmd = currentSchedulingCmds[i];
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
                    vkCmdDraw(
                        vkcmd,
                        args.vertexCount,
                        args.instanceCount,
                        args.firstVertex,
                        args.firstInstance
                    );
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
                        vkBuffers[i] = vkbuf->GetHandle();
                        vkOffsets[i] = args.vertexBufferBindings[i].offset;
                    }

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
                    exeState.lastBindedShader = args.program;
                    exeState.shaderConfig = args.config;
                    exeState.setResources[0].needUpdate = true;
                    exeState.setResources[0].resource = &globalResources[args.program];
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
                    vkCmdSetScissor(
                        vkcmd,
                        args.firstScissor,
                        args.scissorCount,
                        args.rects
                    );
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
                    Gfx::VKRenderPass* renderPass = resourceAllocator->Request(*args.renderPass);
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
                    }
                    exeState.overrideViewport = false;

                    if (!exeState.overrideScissor)
                    {
                        VkRect2D scissor;
                        scissor.offset = {0, 0};
                        scissor.extent = {extent.width, extent.height};
                        vkCmdSetScissor(vkcmd, 0, 1, &scissor);
                    }
                    exeState.overrideScissor = false;

                    vkCmdBeginRenderPass(vkcmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
                    break;
                }
            case Gfx::VKCmdType::BeginLabel:
                {
                    auto& args = std::get<VKBeginLabelCmd>(cmd.args);
                    VKDebugUtils::CmdBeginLabel(vkcmd, args.label, args.color);
                    break;
                }
            case Gfx::VKCmdType::EndLabel:
                {
                    VKDebugUtils::CmdEndLabel(vkcmd);
                    break;
                }
            case Gfx::VKCmdType::InsertLabel:
                {
                    auto& args = std::get<VKInsertLabelCmd>(cmd.args);
                    VKDebugUtils::CmdInsertLabel(vkcmd, args.label, args.color);
                    break;
                }
            case VKCmdType::None: break;
        }
    }

    currentSchedulingCmds.clear();
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

void Graph::UpdateDescriptorSetBinding(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint)
{
    UpdateDescriptorSetBinding(cmd, 0, bindPoint);
    UpdateDescriptorSetBinding(cmd, 1, bindPoint);
    UpdateDescriptorSetBinding(cmd, 2, bindPoint);
    UpdateDescriptorSetBinding(cmd, 3, bindPoint);
}

void Graph::TryBindShader(VkCommandBuffer cmd)
{
    if (exeState.bindedShader != exeState.lastBindedShader && exeState.lastBindedShader != nullptr)
    {

        if (exeState.lastBindedShader->IsCompute())
        {
            auto pipeline = exeState.lastBindedShader->RequestComputePipeline(*exeState.shaderConfig);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        }
        else
        {
            // binding pipeline
            auto pipeline = exeState.lastBindedShader->RequestGraphicsPipeline(
                *exeState.shaderConfig,
                exeState.renderPass,
                exeState.subpassIndex
            );

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        exeState.bindedShader = exeState.lastBindedShader;
    }
}

void Graph::UpdateDescriptorSetBinding(VkCommandBuffer cmd, uint32_t index, VkPipelineBindPoint bindPoint)
{
    if (exeState.setResources[index].needUpdate && exeState.setResources[index].resource)
    {
        auto sourceSet = exeState.setResources[index].resource->GetDescriptorSet(index, exeState.lastBindedShader);
        if (sourceSet != VK_NULL_HANDLE && sourceSet != exeState.bindedDescriptorSets[index])
        {
            vkCmdBindDescriptorSets(
                cmd,
                bindPoint,
                exeState.lastBindedShader->GetVKPipelineLayout(),
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

void Graph::ScheduleBindShaderProgram(VKCmd& cmd, int visitIndex)
{
    ENGINE_SCOPED_PROFILE("ScheduleBindShaderProgram");
    auto& args = std::get<VKBindShaderProgramCmd>(cmd.args);
    if (args.program != recordState.bindedProgram)
    {
        recordState.bindedProgram = args.program;
        recordState.bindProgramIndex = visitIndex;

        auto& resource = globalResources[args.program];
        auto& bindingMap = args.program->GetShaderInfo().descriptorSetBindingMap;
        auto iter = bindingMap.find(0);
        if (iter != bindingMap.end())
        {
            recordState.bindedSetUpdateNeeded[0] = true;
            for (auto& binding : iter->second)
            {
                auto resourceFromPool = globalResourcePool.find(binding->resourceHandle);
                if (resourceFromPool != globalResourcePool.end())
                {
                    for (int elementIndex = 0; elementIndex < binding->count; ++elementIndex)
                    {
                        auto& element = resourceFromPool->second[elementIndex];
                        if (!element.IsNull())
                        {
                            bool isBufferType = (binding->type == ShaderInfo::BindingType::UBO ||
                                                 binding->type == ShaderInfo::BindingType::SSBO) &&
                                                element.type == ResourceType::Buffer;
                            bool isImageType = (binding->type == ShaderInfo::BindingType::SeparateImage ||
                                                binding->type == ShaderInfo::BindingType::Texture ||
                                                binding->type == ShaderInfo::BindingType::StorageImage) &&
                                               element.type == ResourceType::Image;

                            if (isBufferType)
                            {
                                auto& res = std::get<SRef<Buffer>>(element.res);
                                resource.SetBuffer(binding->resourceHandle, elementIndex, res.Get());
                            }
                            else if (isImageType)
                            {
                                auto& res = std::get<SRef<Image>>(element.res);
                                if (element.imageViewOption.has_value())
                                {
                                    auto& imageView = res->GetImageView(*element.imageViewOption);
                                    resource.SetImage(binding->resourceHandle, elementIndex, &imageView);
                                }
                                else
                                {
                                    resource.SetImage(binding->resourceHandle, elementIndex, res.Get());
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

VKImage* Graph::GetImage(const UUID& hash)
{
    return resourceAllocator->GetImage(hash);
}

Graph::Graph()
{
    resourceAllocator = std::make_unique<ResourceAllocator>(this);
}
Graph::~Graph() {}

void Graph::FlushAllBindedSetUpdate(std::vector<VKImage*>& shaderImageSampleIgnoreList, int& barrierCountAdded)
{
    for (int i = 0; i < 4; ++i)
    {
        if (recordState.bindedSetUpdateNeeded[i] == true)
        {
            recordState.bindedSetUpdateNeeded[i] = false;
            auto bindSetCmdIndex = recordState.bindSetCmdIndex[i];
            auto bindProgramIndex = recordState.bindProgramIndex;
            auto& bindProgramArgs = std::get<VKBindShaderProgramCmd>(currentSchedulingCmds[bindProgramIndex].args);
            auto program = bindProgramArgs.program;
            VKBindResourceCmd* bindSetCmd =
                i == 0 ? nullptr : &std::get<VKBindResourceCmd>(currentSchedulingCmds[bindSetCmdIndex].args);
            uint32_t updateSet = i == 0 ? 0 : bindSetCmd->set;
            VKShaderResource* resource = i == 0 ? &globalResources[program] : bindSetCmd->resource;
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
                    VKImage* data = static_cast<VKImage*>(std::get<SRef<Image>>(w.data).Get());

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
                    VKBuffer* data = static_cast<VKBuffer*>(std::get<SRef<Buffer>>(w.data).Get());
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

} // namespace Gfx::VK::RenderGraph
