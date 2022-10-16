#pragma once
#include "../RenderPass.hpp"
#include "VKFrameBuffer.hpp"
#include <vector>
#include <optional>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
{
    class VKContext;
    class VKImage;
    class VKRenderPass : public RenderPass
    {
        public:
            VKRenderPass();
            VKRenderPass(const VKRenderPass& renderPass) = delete;
            VKRenderPass(VKRenderPass&& renderPass) = delete;
            ~VKRenderPass() override;
            void AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) override;

            void TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf);


            void GetHandle(VkRenderPass& renderPass, VkFramebuffer& frameBuffer);
            Extent2D GetExtent();
        protected:
            void CreateRenderPass();
            void CreateFrameBuffer();

            VkRenderPass renderPass = VK_NULL_HANDLE;
            VkFramebuffer frameBuffer = VK_NULL_HANDLE;

            struct Subpass
            {
                Subpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) : colors(colors), depth(depth) {}
                std::vector<Attachment> colors;
                std::optional<Attachment> depth;
            };
            std::vector<Subpass> subpasses;
    };
}
