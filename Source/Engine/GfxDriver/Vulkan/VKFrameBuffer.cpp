#include "VKFrameBuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKImage.hpp"
#include "VKContext.hpp"
#include "Internal/VKObjectManager.hpp"
namespace Engine::Gfx
{
    VKFrameBuffer::VKFrameBuffer(RefPtr<VKContext> context, RefPtr<RenderPass> baseRenderPass) : baseRenderPass((VKRenderPass*)baseRenderPass.Get()), context(context)
    {}

    VKFrameBuffer::~VKFrameBuffer()
    {
        if (frameBuffer != VK_NULL_HANDLE)
            context->objManager->DestroyFramebuffer(frameBuffer);
    }

    void VKFrameBuffer::SetAttachments(const std::vector<RefPtr<Image>> &attachments)
    {
        this->attachments.clear();
        for (auto a : attachments)
        {
            this->attachments.push_back((VKImage *)a.Get());
        }

        if (!this->attachments.empty())
        {
            width = this->attachments[0]->GetDescription().width;
            height = this->attachments[0]->GetDescription().height;
        }
    }

    VkFramebuffer VKFrameBuffer::GetHandle()
    {
        if (frameBuffer == VK_NULL_HANDLE)
        {
            CreateFrameBuffer();
        }

        return frameBuffer;
    }

    void VKFrameBuffer::CreateFrameBuffer()
    {
        VkFramebufferCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.pNext = VK_NULL_HANDLE;
        createInfo.flags = 0;
        createInfo.renderPass = baseRenderPass->GetHandle();

        assert(attachments.size() <= 32);
        VkImageView imageViews[32] = {};
        uint32_t i = 0;
        for (auto a : attachments)
        {
            imageViews[i] = a->GetDefaultImageView();
            i += 1;
        }
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments = imageViews;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = 1;

        context->objManager->CreateFramebuffer(createInfo, frameBuffer);
    }
}
