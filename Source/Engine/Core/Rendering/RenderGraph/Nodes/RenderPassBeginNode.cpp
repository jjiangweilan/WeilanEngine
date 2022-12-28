#include "RenderPassBeginNode.hpp"

namespace Engine::RGraph
{
RenderPassBeginNode::RenderPassBeginNode()
{
    Port* color0 = AddPort("Color 0", Port::Type::Input, typeid(Gfx::Image));
    colorPorts.push_back(color0);
    depthPort = AddPort("Depth", Port::Type::Input, typeid(Gfx::Image));
    renderPassOut = AddPort("Out", Port::Type::Output, typeid(Gfx::RenderPass));
    commandBufferPortIn = AddPort("cmdBuf", Port::Type::Input, typeid(CommandBuffer));
    commandBufferPortOut = AddPort("cmdBuf", Port::Type::Output, typeid(CommandBuffer));

    renderPass = GetGfxDriver()->CreateRenderPass();
}

bool RenderPassBeginNode::Preprocess(ResourceStateTrack& stateTrack)
{
    for (auto& c : colorPorts)
    {
        auto colorSourcePort = c->GetConnectedPort();
        if (colorSourcePort)
        {
            stateTrack.GetState(colorSourcePort->GetResource()).usages |= Gfx::ImageUsage::ColorAttachment;
        }
    }

    if (auto depthSourcePort = depthPort->GetConnectedPort())
    {
        auto& state = stateTrack.GetState(depthSourcePort->GetResource());
        state.usages |= Gfx::ImageUsage::DepthStencilAttachment;
    }

    return true;
};

bool RenderPassBeginNode::Compile(ResourceStateTrack& stateTrack)
{
    cmdBuf = (CommandBuffer*)GetResourceFromConnected(commandBufferPortIn);
    if (!cmdBuf) return false;
    commandBufferPortOut->GetResource().SetVal(cmdBuf);

    int colorCount = 0;
    int hasDepth = 0;

    for (auto& c : colorPorts)
    {
        if (!c->GetConnectedPort()->GetResource().IsNull()) colorCount += 1;
    }
    if (!depthPort->GetConnectedPort()->GetResource().IsNull()) hasDepth = 1;

    // at least one attachment is needed
    if (colorCount + hasDepth == 0) return false;

    colorAttachments.resize(colorCount);
    assert(colorAttachmentOps.size() == colorCount); // this is required for the following code path

    // fill attachment data
    int attachmentIndex = 0;
    for (auto& c : colorPorts)
    {
        if (!c->GetConnectedPort()->GetResource().IsNull())
        {
            Gfx::Image* colorImage = (Gfx::Image*)c->GetConnectedPort()->GetResource().GetVal();
            Gfx::RenderPass::Attachment& a = colorAttachments[attachmentIndex];
            a.image = colorImage;
            colorAttachmentOps[attachmentIndex].FillAttachment(a);
        }
    }

    if (hasDepth)
    {
        depthAttachment->image = (Gfx::Image*)depthPort->GetConnectedPort()->GetResource().GetVal();
        depthAttachmentOp.FillAttachment(*depthAttachment);
    }
    else depthAttachment = std::nullopt;

    renderPass->AddSubpass(colorAttachments, depthAttachment);

    return true;
}

bool RenderPassBeginNode::Execute(ResourceStateTrack& stateTrack)
{
    Port* colorPort0 = colorPorts.front();
    ResourceRef color0Res = colorPort0->GetConnectedPort()->GetResource();
    ResourceRef depthRes = depthPort->GetConnectedPort()->GetResource();

    // render pass need a color attachment
    if (color0Res.IsNull()) return false;

    auto& resourceState = stateTrack.GetState(color0Res);
    GPUBarrier barriers[2];
    int barrierIndex = 0;

    if (resourceState.layout != Gfx::ImageLayout::Color_Attachment)
    {
        barriers[barrierIndex].image = (Gfx::Image*)color0Res.GetVal();
        barriers[barrierIndex].imageInfo.srcQueueFamilyIndex = resourceState.queueFamilyIndex;
        barriers[barrierIndex].imageInfo.dstQueueFamilyIndex = resourceState.queueFamilyIndex;
        barriers[barrierIndex].imageInfo.oldLayout = resourceState.layout;
        barriers[barrierIndex].imageInfo.newLayout = Gfx::ImageLayout::Color_Attachment;
        barriers[barrierIndex].dstStageMask = Gfx::PipelineStage::Color_Attachment_Output;
        barriers[barrierIndex].dstAccessMask =
            Gfx::AccessMask::Color_Attachment_Read | Gfx::AccessMask::Color_Attachment_Write;
        barriers[barrierIndex].srcStageMask = resourceState.stage;
        barriers[barrierIndex].srcAccessMask = resourceState.accessMask;

        // set the new state
        ResourceState newState;
        newState.stage = barriers[barrierIndex].dstStageMask;
        newState.layout = barriers[barrierIndex].imageInfo.newLayout;
        newState.accessMask = barriers[barrierIndex].dstAccessMask;
        newState.stage = barriers[barrierIndex].dstStageMask;
        stateTrack.GetState(color0Res) = newState;

        barrierIndex += 1;
    }

    if (!depthRes.IsNull() && resourceState.layout != Gfx::ImageLayout::Depth_Stencil_Attachment)
    {
        barriers[barrierIndex].image = (Gfx::Image*)depthRes.GetVal();
        barriers[barrierIndex].imageInfo.srcQueueFamilyIndex = resourceState.queueFamilyIndex;
        barriers[barrierIndex].imageInfo.dstQueueFamilyIndex = resourceState.queueFamilyIndex;
        barriers[barrierIndex].imageInfo.oldLayout = resourceState.layout;
        barriers[barrierIndex].imageInfo.newLayout = Gfx::ImageLayout::Depth_Stencil_Attachment;
        barriers[barrierIndex].dstStageMask =
            Gfx::PipelineStage::Early_Fragment_Tests | Gfx::PipelineStage::Late_Fragment_Tests;
        barriers[barrierIndex].dstAccessMask =
            Gfx::AccessMask::Depth_Stencil_Attachment_Read | Gfx::AccessMask::Depth_Stencil_Attachment_Write;
        barriers[barrierIndex].srcStageMask = resourceState.stage;
        barriers[barrierIndex].srcAccessMask = resourceState.accessMask;

        // set the new state
        ResourceState newState;
        newState.stage = barriers[barrierIndex].dstStageMask;
        newState.layout = barriers[barrierIndex].imageInfo.newLayout;
        newState.accessMask = barriers[barrierIndex].dstAccessMask;
        newState.stage = barriers[barrierIndex].dstStageMask;
        stateTrack.GetState(depthRes) = newState;

        barrierIndex += 1;
    }

    cmdBuf->Barrier(barriers, barrierIndex);
    cmdBuf->BeginRenderPass(renderPass, clearValues);

    return true;
}

void RenderPassBeginNode::SetColorCount(uint32_t count)
{
    uint32_t val = count;
    uint32_t oldVal = colorAttachments.size();
    if (val > 8) return;
    if (val > oldVal)
    {
        for (int i = oldVal + 1; i < val; ++i)
        {
            colorPorts.push_back(AddPort("Color", Port::Type::Input, typeid(Gfx::Image)));
        }
    }
    else if (val < oldVal)
    {
        for (int i = oldVal; i >= val; --i)
        {
            auto port = colorPorts.back();
            RemovePort(port);
            colorPorts.pop_back();
        }
    }

    colorAttachmentOps.resize(colorPorts.size());
}
} // namespace Engine::RGraph
