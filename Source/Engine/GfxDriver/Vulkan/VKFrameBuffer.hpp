#pragma once
#include "../FrameBuffer.hpp"
#include "Libs/Ptr.hpp"
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    class VKRenderPass;
    class VKImage;
    class VKFrameBuffer : public FrameBuffer
    {
        public:
            VKFrameBuffer(RefPtr<RenderPass> baseRenderPass);
            VKFrameBuffer(const VKFrameBuffer& other) = delete;
            ~VKFrameBuffer() override;

            void SetAttachments(const std::vector<RefPtr<Image>>& attachments) override;

            std::vector<RefPtr<VKImage>>& GetAttachments() { return attachments; };
            uint32_t GetWidth() {return width;}
            uint32_t GetHeight() {return height;}
            VkFramebuffer GetHandle();
        private:

            uint32_t width;
            uint32_t height;
            VkFramebuffer frameBuffer = VK_NULL_HANDLE;
            RefPtr<VKRenderPass> baseRenderPass;
            
            std::vector<RefPtr<VKImage>> attachments;
            void CreateFrameBuffer();
    };
}
