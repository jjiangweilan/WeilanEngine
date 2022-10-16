#pragma once
#include "../RenderPass.hpp"
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
            void AddSubpass(const std::vector<RefPtr<Attachment>>& colors, RefPtr<Attachment> depth) override;

            void TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf, std::vector<RefPtr<VKImage>>& attachments);

            VkRenderPass GetHandle();
            VkFramebuffer GetFrameBuffer();
        protected:
            std::vector<VkAttachmentDescription> colorAttachments;
            std::optional<VkAttachmentDescription> depthAttachment;

            struct SubpassDescriptionData
            {
                VkSubpassDescription subpass;

                std::vector<VkAttachmentReference> inputs;
                std::vector<VkAttachmentReference> colors;
                std::vector<VkAttachmentReference> resolves;
                VkAttachmentReference depthStencil;
            };
            std::vector<SubpassDescriptionData> subpassDescriptions;
            std::vector<VkSubpassDependency> dependencies;

            // to keep code clean, ubpass is processed inplace when the renderPass object is created
            void CreateRenderPass();

            VkRenderPass renderPass = VK_NULL_HANDLE;




            std::vector<RefPtr<Image>> colors;
            std::vector<RefPtr<Image>> depths;

            struct Subpass
            {
                std::vector<RefPtr<Image>> colors;
                RefPtr<Image> depth;
            };
            std::vector<Subpass> subpasses;
    };
}
