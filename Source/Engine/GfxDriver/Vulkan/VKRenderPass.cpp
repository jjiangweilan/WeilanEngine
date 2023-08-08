#include "VKRenderPass.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKUtils.hpp"
#include "VKContext.hpp"
#include "VKImage.hpp"
#include "VKImageView.hpp"
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
VKRenderPass::VKRenderPass() {}

VKRenderPass::~VKRenderPass()
{
    if (renderPass != VK_NULL_HANDLE)
        VKContext::Instance()->objManager->DestroyRenderPass(renderPass);

    for (auto fb : frameBuffers)
        VKContext::Instance()->objManager->DestroyFramebuffer(fb);
}

void VKRenderPass::AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth)
{
    subpasses.emplace_back(colors, depth);
}

VkFramebuffer VKRenderPass::CreateFrameBuffer()
{
    VkFramebufferCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.renderPass = renderPass;

    VkImageView imageViews[64] = {};
    uint32_t attaIndex = 0;
    // same way to order pAttachments as in CreateRenderPass
    for (auto& subpass : subpasses)
    {
        // color attachments
        for (Attachment& colorAtta : subpass.colors)
        {
            auto& imageView = *static_cast<VKImageView*>(colorAtta.imageView);
            if (swapChainProxy == nullptr)
            {
                swapChainProxy = dynamic_cast<VKSwapChainImageProxy*>(colorAtta.imageView->GetImage());
            }
            imageViews[attaIndex] = imageView.GetHandle();
            attaIndex += 1;
        }

        // depth attachment
        if (subpass.depth != std::nullopt)
        {
            imageViews[attaIndex] = static_cast<VKImageView*>(subpass.depth->imageView)->GetHandle();
            attaIndex += 1;
        }
    }
    createInfo.attachmentCount = attaIndex;
    createInfo.pAttachments = imageViews;

    Gfx::Image* image = subpasses[0].colors.empty() ? nullptr : subpasses[0].colors[0].imageView->GetImage();
    if (image == nullptr)
        image = subpasses[0].depth->imageView->GetImage();
    createInfo.width = image->GetDescription().width;
    createInfo.height = image->GetDescription().height;
    createInfo.layers = 1;

    VkFramebuffer newFrameBuffer = VK_NULL_HANDLE;
    VKContext::Instance()->objManager->CreateFramebuffer(createInfo, newFrameBuffer);
    return newFrameBuffer;
}

void VKRenderPass::CreateRenderPass()
{
    VkRenderPassCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;

    VkAttachmentDescription attachmentDescriptions[64];

    uint32_t attachmentCount = 0;
    for (auto& subpass : subpasses)
    {
        attachmentCount += subpass.colors.size();
        if (subpass.depth != std::nullopt)
        {
            attachmentCount += 1;
        }
    }
    assert(attachmentCount <= 64);

    createInfo.attachmentCount = attachmentCount;
    createInfo.pAttachments = attachmentDescriptions;

    VkSubpassDescription subpassDescriptions[64];
    VkAttachmentReference attachmentReference[128];
    assert(subpasses.size() <= 64);

    uint32_t subpassDescIndex = 0;
    uint32_t attachmentDescIndex = 0;
    uint32_t refIndex = 0;
    for (auto& subpass : subpasses)
    {
        subpassDescriptions[subpassDescIndex].flags = 0;
        subpassDescriptions[subpassDescIndex].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescriptions[subpassDescIndex].inputAttachmentCount = 0;
        subpassDescriptions[subpassDescIndex].pInputAttachments = VK_NULL_HANDLE;
        subpassDescriptions[subpassDescIndex].pResolveAttachments = VK_NULL_HANDLE;
        subpassDescriptions[subpassDescIndex].preserveAttachmentCount = 0;
        subpassDescriptions[subpassDescIndex].pPreserveAttachments = VK_NULL_HANDLE;

        // color attachments
        subpassDescriptions[subpassDescIndex].colorAttachmentCount = subpass.colors.size();
        subpassDescriptions[subpassDescIndex].pColorAttachments = attachmentReference + refIndex;
        for (Attachment& colorAtta : subpass.colors)
        {
            attachmentDescriptions[attachmentDescIndex].flags = 0;
            auto image = colorAtta.imageView->GetImage();
            auto format = MapFormat(image->GetDescription().format);
            attachmentDescriptions[attachmentDescIndex].format = format;

            attachmentDescriptions[attachmentDescIndex].samples = MapSampleCount(image->GetDescription().multiSampling);
            attachmentDescriptions[attachmentDescIndex].loadOp = MapAttachmentLoadOp(colorAtta.loadOp);
            attachmentDescriptions[attachmentDescIndex].storeOp = MapAttachmentStoreOp(colorAtta.storeOp);
            attachmentDescriptions[attachmentDescIndex].stencilLoadOp = MapAttachmentLoadOp(colorAtta.stencilLoadOp);
            attachmentDescriptions[attachmentDescIndex].stencilStoreOp = MapAttachmentStoreOp(colorAtta.storeOp);
            attachmentDescriptions[attachmentDescIndex].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescIndex].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentReference[refIndex].attachment = attachmentDescIndex;
            attachmentReference[refIndex].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            attachmentDescIndex += 1;
            refIndex += 1;
        }

        // depth attachment
        if (subpass.depth != std::nullopt)
        {
            subpassDescriptions[subpassDescIndex].pDepthStencilAttachment = attachmentReference + refIndex;

            attachmentDescriptions[attachmentDescIndex].flags = 0;
            attachmentDescriptions[attachmentDescIndex].format =
                MapFormat(subpass.depth->imageView->GetImage()->GetDescription().format);
            attachmentDescriptions[attachmentDescIndex].samples =
                MapSampleCount(subpass.depth->imageView->GetImage()->GetDescription().multiSampling);
            attachmentDescriptions[attachmentDescIndex].loadOp = MapAttachmentLoadOp(subpass.depth->loadOp);
            attachmentDescriptions[attachmentDescIndex].storeOp = MapAttachmentStoreOp(subpass.depth->storeOp);
            attachmentDescriptions[attachmentDescIndex].stencilLoadOp =
                MapAttachmentLoadOp(subpass.depth->stencilLoadOp);
            attachmentDescriptions[attachmentDescIndex].stencilStoreOp = MapAttachmentStoreOp(subpass.depth->storeOp);
            attachmentDescriptions[attachmentDescIndex].initialLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachmentDescriptions[attachmentDescIndex].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            attachmentReference[refIndex].attachment = attachmentDescIndex;
            attachmentReference[refIndex].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            attachmentDescIndex += 1;
            refIndex += 1;
        }
        else
        {
            subpassDescriptions[subpassDescIndex].pDepthStencilAttachment = VK_NULL_HANDLE;
        }

        if (subpassDescIndex != 0)
        {}

        subpassDescIndex += 1;
    }

    createInfo.subpassCount = subpasses.size();
    createInfo.pSubpasses = subpassDescriptions;

    VkSubpassDependency externalDependency;
    externalDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    externalDependency.dstSubpass = 0;
    externalDependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
    externalDependency.dstStageMask = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    externalDependency.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    externalDependency.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    externalDependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &externalDependency;

    VKContext::Instance()->objManager->CreateRenderPass(createInfo, renderPass);
}

Extent2D VKRenderPass::GetExtent()
{
    if (subpasses.size() > 0)
    {
        if (subpasses[0].colors.size() > 0)
        {
            auto& desc = subpasses[0].colors[0].imageView->GetImage()->GetDescription();
            return {desc.width, desc.height};
        }
        else
        {
            if (subpasses[0].depth.has_value())
            {
                auto& desc = subpasses[0].depth->imageView->GetImage()->GetDescription();
                return {desc.width, desc.height};
            }
        }
    }

    SPDLOG_WARN("VKRenderPass-GetExtent returns {0, 0}");
    return {0, 0};
}

VkFramebuffer VKRenderPass::GetFrameBuffer()
{
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (frameBuffers.empty() ||
        (swapChainProxy != nullptr && (frameBuffers.size() <= swapChainProxy->GetActiveIndex() ||
                                       frameBuffers[swapChainProxy->GetActiveIndex()] == VK_NULL_HANDLE)))
    {
        framebuffer = CreateFrameBuffer();
        size_t index = swapChainProxy ? swapChainProxy->GetActiveIndex() : 0;

        if (frameBuffers.size() <= index)
            frameBuffers.resize(index + 1);

        frameBuffers[index] = framebuffer;
    }
    else
    {
        if (swapChainProxy != nullptr)
            framebuffer = frameBuffers[swapChainProxy->GetActiveIndex()];
        else
            framebuffer = frameBuffers[0];
    }

    return framebuffer;
}

VkRenderPass VKRenderPass::GetHandle()
{
    if (renderPass == VK_NULL_HANDLE)
    {
        CreateRenderPass();
    }

    return renderPass;
}

} // namespace Engine::Gfx
