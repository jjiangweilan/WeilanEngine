#include "VKRenderPass.hpp"
#include "VKContext.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKUtils.hpp"
#include "VKImage.hpp"
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    VKRenderPass::VKRenderPass()
    {
    }

    VKRenderPass::~VKRenderPass()
    {
        if (renderPass != VK_NULL_HANDLE)
            VKContext::Instance()->objManager->DestroyRenderPass(renderPass);

        if (frameBuffer != VK_NULL_HANDLE)
            VKContext::Instance()->objManager->DestroyFramebuffer(frameBuffer);
    }

    void VKRenderPass::AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth)
    {
        subpasses.emplace_back(colors, depth);
    }

    void VKRenderPass::CreateFrameBuffer()
    {
        VkFramebufferCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;
        createInfo.renderPass = renderPass;

        VkImageView imageViews[64] = {};
        uint32_t attaIndex = 0;
        // same way to order pAttachments as in CreateRenderPass
        for(auto& subpass : subpasses)
        {
            // color attachments
            for(Attachment& colorAtta : subpass.colors)
            {
                imageViews[attaIndex] = static_cast<VKImage*>(colorAtta.image.Get())->GetDefaultImageView();
                attaIndex += 1;
            }

            // depth attachment
            if (subpass.depth != std::nullopt)
            {
                imageViews[attaIndex] = static_cast<VKImage*>(subpass.depth->image.Get())->GetDefaultImageView();
                attaIndex += 1;
            }

        }
        createInfo.attachmentCount = attaIndex;
        createInfo.pAttachments = imageViews;
        createInfo.width = subpasses[0].colors[0].image->GetDescription().width;
        createInfo.height = subpasses[0].colors[0].image->GetDescription().height;
        createInfo.layers = 1;

        VKContext::Instance()->objManager->CreateFramebuffer(createInfo, frameBuffer);
    }

    void VKRenderPass::CreateRenderPass()
    {
        VkRenderPassCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;

        VkAttachmentDescription attachmentDescriptions[64];

        uint32_t attachmentCount = 0;
        for(auto& subpass : subpasses)
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
        for(auto& subpass : subpasses)
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
            for(Attachment& colorAtta : subpass.colors)
            {
                attachmentDescriptions[attachmentDescIndex].flags = 0;
                attachmentDescriptions[attachmentDescIndex].format = VKEnumMapper::MapFormat(colorAtta.image->GetDescription().format);
                attachmentDescriptions[attachmentDescIndex].samples = VKEnumMapper::MapSampleCount(colorAtta.image->GetDescription().multiSampling);
                attachmentDescriptions[attachmentDescIndex].loadOp = VKEnumMapper::MapAttachmentLoadOp(colorAtta.loadOp);
                attachmentDescriptions[attachmentDescIndex].storeOp = VKEnumMapper::MapAttachmentStoreOp(colorAtta.storeOp);
                attachmentDescriptions[attachmentDescIndex].stencilLoadOp = VKEnumMapper::MapAttachmentLoadOp(colorAtta.stencilLoadOp);
                attachmentDescriptions[attachmentDescIndex].stencilStoreOp = VKEnumMapper::MapAttachmentStoreOp(colorAtta.storeOp);
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
                attachmentDescriptions[attachmentDescIndex].format = VKEnumMapper::MapFormat(subpass.depth->image->GetDescription().format);
                attachmentDescriptions[attachmentDescIndex].samples = VKEnumMapper::MapSampleCount(subpass.depth->image->GetDescription().multiSampling);
                attachmentDescriptions[attachmentDescIndex].loadOp = VKEnumMapper::MapAttachmentLoadOp(subpass.depth->loadOp);
                attachmentDescriptions[attachmentDescIndex].storeOp = VKEnumMapper::MapAttachmentStoreOp(subpass.depth->storeOp);
                attachmentDescriptions[attachmentDescIndex].stencilLoadOp = VKEnumMapper::MapAttachmentLoadOp(subpass.depth->stencilLoadOp);
                attachmentDescriptions[attachmentDescIndex].stencilStoreOp = VKEnumMapper::MapAttachmentStoreOp(subpass.depth->storeOp);
                attachmentDescriptions[attachmentDescIndex].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
            {
            }

            subpassDescIndex += 1;
        }

        createInfo.subpassCount = subpasses.size();
        createInfo.pSubpasses = subpassDescriptions;

        createInfo.dependencyCount = 0;
        createInfo.pDependencies = VK_NULL_HANDLE;

        VKContext::Instance()->objManager->CreateRenderPass(createInfo, renderPass);
    }


    Extent2D VKRenderPass::GetExtent()
    {
        if (subpasses.size() > 0 && subpasses[0].colors.size() > 0 && subpasses[0].colors[0].image != nullptr)
        {
            auto& desc = subpasses[0].colors[0].image->GetDescription();
            return { desc.width, desc.height };
        }

        return {0,0};
    }

    void VKRenderPass::TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf)
    {
        for(auto& subpass : subpasses)
        {
            for(auto& atta : subpass.colors)
            {
                auto image = static_cast<VKImage*>(atta.image.Get());
                VkAccessFlags accessFlag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                if (atta.loadOp == AttachmentLoadOperation::Load) {
                    accessFlag |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                }
                image->TransformLayoutIfNeeded(cmdBuf, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, accessFlag);
            }

            if (subpass.depth != std::nullopt)
            {
                auto image = static_cast<VKImage*>(subpass.depth->image.Get());
                VkAccessFlags accessFlag = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                if (subpass.depth->loadOp == AttachmentLoadOperation::Load || subpass.depth->stencilLoadOp == AttachmentLoadOperation::Load) accessFlag |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

                image->TransformLayoutIfNeeded(cmdBuf, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, accessFlag);
            }
        }
    }

    void VKRenderPass::GetHandle(VkRenderPass& renderPass, VkFramebuffer& frameBuffer)
    {
        if (this->renderPass == VK_NULL_HANDLE)
        {
            CreateRenderPass();
        }

        if (this->frameBuffer == VK_NULL_HANDLE)
        {
            CreateFrameBuffer();
        }

        renderPass = this->renderPass;
        frameBuffer = this->frameBuffer;
    }

}
