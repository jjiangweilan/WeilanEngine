#include "VKRenderGraph.hpp"
#include "../VKUtils.hpp"

namespace Gfx::VK::RenderGraph
{

void BuildDependencyGraph() {}

void Graph::TrackResource(
    int renderingOpIndex,
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
        iter->second.usages.push_back({stages, access, renderingOpIndex, range, layout});
    }
    else
    {
        ResourceUsageTrack track;
        track.type = type;
        track.res = writableResource;
        track.usages.push_back({stages, access, renderingOpIndex, range, layout});

        resourceUsageTracks[writableResource] = track;
    }
}
void Graph::RegisterRenderPass(int& visitIndex)
{
    assert(activeSchedulingCmds[visitIndex].type == VKCmdType::BeginRenderPass);
    auto& cmd = activeSchedulingCmds[visitIndex].beginRenderPass;
    int cmdBeginIndex = visitIndex;

    RenderingNode node{RenderingNodeType::RenderPass};
    int renderingOpIndex = renderingNodes.size();

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
                renderingOpIndex,
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
                renderingOpIndex,
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
    // proceed to End Render Pass
    for (;;)
    {
        visitIndex += 1;
        auto& cmd = activeSchedulingCmds[visitIndex];
        if (cmd.type == VKCmdType::EndRenderPass)
            break;
        else if (cmd.type == VKCmdType::BindResource)
        {
            state.bindSetCmdIndex[cmd.bindResource.set] = visitIndex;
            state.bindedSetUpdateNeeded[cmd.bindResource.set] = true;
        }
        else if (cmd.type == VKCmdType::BindShaderProgram)
        {
            state.bindProgramIndex = visitIndex;
        }
        else if (cmd.type == VKCmdType::Draw || cmd.type == VKCmdType::DrawIndexed)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (state.bindedSetUpdateNeeded[i] == true)
                {
                    auto bindSetCmdIndex = state.bindSetCmdIndex[i];
                    auto bindProgramIndex = state.bindProgramIndex;
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
                                renderingOpIndex,
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
                            TrackResource(
                                renderingOpIndex,
                                type,
                                w.data,
                                Gfx::ImageSubresourceRange(),
                                w.layout,
                                w.stages,
                                w.access
                            );
                        }

                        barrierCount += MakeBarrier(w.data, resourceUsageTracks.size() - 1);
                    }
                }
            }
        }
        else if (visitIndex >= activeSchedulingCmds.size())
            break;
    }

    node.renderPass.cmdBegin = cmdBeginIndex;
    node.renderPass.cmdEnd = visitIndex;
    node.renderPass.barrierOffset = barrierOffset;
    node.renderPass.barrierCount = barrierCount;

    renderingNodes.push_back(node);
}

int Graph::MakeBarrier(void* res, int usageIndex)
{
    auto iter = resourceUsageTracks.find(res);
    assert(iter != resourceUsageTracks.end());

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

                    if (srcStages != Gfx::PipelineStage::None || srcAccess != Gfx::AccessMask::None ||
                        preUsedDesc.imageLayout != desc.imageLayout)
                    {
                        auto image = (Gfx::Image*)r->resourceRef.GetResource();
                        if (srcStages == Gfx::PipelineStage::None)
                            srcStages = Gfx::PipelineStage::Top_Of_Pipe;
                        Gfx::GPUBarrier barrier{
                            .image = image,
                            .srcStageMask = srcStages,
                            .dstStageMask = desc.stageFlags,
                            .srcAccessMask = srcAccess,
                            .dstAccessMask = desc.accessFlags,
                            .imageInfo = {
                                .srcQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                                .dstQueueFamilyIndex = GFX_QUEUE_FAMILY_IGNORED,
                                .oldLayout = preUsedDesc.imageLayout,
                                .newLayout = desc.imageLayout,
                                .subresourceRange = overlapping}};

                        barriers[sortIndex].push_back(barrier);
                    }

                    auto remainings = currentRange.Subtract(preUsedRange);
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
}

void Graph::FlushBindResourceTrack() {}

void Graph::Schedule(VKCommandBuffer2& cmd)
{
    int cmdIndexOffset = activeSchedulingCmds.size();
    activeSchedulingCmds.insert(activeSchedulingCmds.end(), cmd.GetCmds().begin(), cmd.GetCmds().end());

    // register rendering operation
    for (int visitIndex = cmdIndexOffset; visitIndex < activeSchedulingCmds.size(); visitIndex++)
    {
        auto& cmd = activeSchedulingCmds[visitIndex];
        switch (cmd.type)
        {
            case VKCmdType::BeginRenderPass: RegisterRenderPass(visitIndex); break;
            case VKCmdType::EndRenderPass: break;
        }
    }
}
} // namespace Gfx::VK::RenderGraph
