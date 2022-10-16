#include "VKRenderPass.hpp"
#include "VKContext.hpp"
#include "Internal/VKObjectManager.hpp"
#include "Internal/VKEnumMapper.hpp"
#include "Internal/VKUtils.hpp"
#include "VKImage.hpp"
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    VKRenderPass::VKRenderPass() :
        colorAttachments(),
        subpassDescriptions(),
        dependencies()
    {

    }

    VKRenderPass::~VKRenderPass()
    {
        if (renderPass != VK_NULL_HANDLE)
            VKContext::Instance()->objManager->DestroyRenderPass(renderPass);
    }


    void VKRenderPass::SetAttachments(const std::vector<Attachment>& colors, std::optional<Attachment> depth)
    {
        colorAttachments.clear();
        for(auto& attachment : colors)
        {
            VkAttachmentDescription desc;
            desc.flags = 0;
            desc.format = VKEnumMapper::MapFormat(attachment.format);
            desc.samples = VKEnumMapper::MapSampleCount(attachment.multiSampling);
            desc.loadOp = VKEnumMapper::MapAttachmentLoadOp(attachment.loadOp);
            desc.storeOp = VKEnumMapper::MapAttachmentStoreOp(attachment.storeOp);
            desc.stencilLoadOp = VKEnumMapper::MapAttachmentLoadOp(attachment.stencilLoadOp);
            desc.stencilStoreOp = VKEnumMapper::MapAttachmentStoreOp(attachment.storeOp);
            desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments.push_back(desc);
        }

        if (depth.has_value())
        {
            depthAttachment = VkAttachmentDescription();
            depthAttachment->flags = 0;
            depthAttachment->format = VKEnumMapper::MapFormat(depth->format);
            depthAttachment->samples = VKEnumMapper::MapSampleCount(depth->multiSampling);
            depthAttachment->loadOp = VKEnumMapper::MapAttachmentLoadOp(depth->loadOp);
            depthAttachment->storeOp = VKEnumMapper::MapAttachmentStoreOp(depth->storeOp);
            depthAttachment->stencilLoadOp = VKEnumMapper::MapAttachmentLoadOp(depth->stencilLoadOp);
            depthAttachment->stencilStoreOp = VKEnumMapper::MapAttachmentStoreOp(depth->storeOp);
            depthAttachment->initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment->finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        }
    }

    void VKRenderPass::SetSubpass(const std::vector<Subpass>& subpasses)
    {
        subpassDescriptions.clear();
        subpassDescriptions.resize(subpasses.size());
        uint32_t i = 0;
        for(auto& subpass : subpasses)
        {
            SubpassDescriptionData& data = subpassDescriptions[i];
            data.subpass.flags = 0;
            data.subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            for(uint32_t input : subpass.inputs)
            {
                VkAttachmentReference r;
                r.attachment = input;
                r.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                data.inputs.push_back(r);
            }

            for(uint32_t color : subpass.colors)
            {
                VkAttachmentReference r;
                r.attachment = color;
                r.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                data.colors.push_back(r);
            }


            data.resolves.clear();
            data.subpass.colorAttachmentCount = data.colors.size();
            data.subpass.pColorAttachments = data.colors.data();
            data.subpass.flags = 0;
            data.subpass.inputAttachmentCount = data.inputs.size();
            data.subpass.pInputAttachments = data.inputs.data();
            if (subpass.depthAttachment == -1)
                data.subpass.pDepthStencilAttachment = VK_NULL_HANDLE;
            else
            {
                data.depthStencil.attachment = subpass.depthAttachment;
                // actual value is resolved in CreateRenderPass
                data.depthStencil.layout = VK_IMAGE_LAYOUT_UNDEFINED;
                data.subpass.pDepthStencilAttachment = &data.depthStencil;
            }
            data.subpass.preserveAttachmentCount = 0;
            data.subpass.pPreserveAttachments = VK_NULL_HANDLE;
            data.subpass.pResolveAttachments = VK_NULL_HANDLE;

            i += 1;
        }
    }

    void VKRenderPass::AddSubpass(std::vector<RefPtr<Image>>&& colors, RefPtr<Image> depth)
    {
    }

    void VKRenderPass::CreateRenderPass()
    {
        VkRenderPassCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;

        if (depthAttachment.has_value())
        {
            colorAttachments.reserve(colorAttachments.size() + 1);
            colorAttachments.push_back(depthAttachment.value());
        }

        createInfo.attachmentCount = colorAttachments.size();
        createInfo.pAttachments = colorAttachments.data();

        std::vector<VkSubpassDescription> finalSubpassDescriptions(subpassDescriptions.size());

        uint32_t i = 0;
        for(auto& v : subpassDescriptions)
        {
            // we don't use implicit subpass dependency between renderpass, so the finalLayout and the initialLayout are set to the same value
            v.depthStencil.layout = depthAttachment != std::nullopt ? depthAttachment->finalLayout : VK_IMAGE_LAYOUT_UNDEFINED;
            finalSubpassDescriptions[i] = v.subpass;
        }

        createInfo.subpassCount = finalSubpassDescriptions.size();
        createInfo.pSubpasses = finalSubpassDescriptions.data();

        createInfo.dependencyCount = 0;
        createInfo.pDependencies = VK_NULL_HANDLE;

        VKContext::Instance()->objManager->CreateRenderPass(createInfo, renderPass);
    }

    void VKRenderPass::TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf, std::vector<RefPtr<VKImage>>& attachments)
    {
        if (subpassDescriptions.size() > 0)
        {
            auto& desc = subpassDescriptions[0];
            for(auto cRef : desc.colors)
            {
                auto subRscRange = attachments[cRef.attachment]->GetDefaultSubresourceRange();
                attachments[cRef.attachment]->TransformLayoutIfNeeded(cmdBuf, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, subRscRange);
            }

            if (depthAttachment.has_value())
            {
                auto& depth = attachments[desc.depthStencil.attachment];
                auto subRscRange = depth->GetDefaultSubresourceRange();
                depth->TransformLayoutIfNeeded(cmdBuf, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, subRscRange);
            }
        }
    }

    VkRenderPass VKRenderPass::GetHandle()
    {
        if (renderPass == VK_NULL_HANDLE)
        {
            CreateRenderPass();
        }

        return renderPass;
    }

}
