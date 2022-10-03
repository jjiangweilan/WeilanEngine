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
            void SetAttachments(const std::vector<Attachment>& colors, std::optional<Attachment> depth) override;
            void SetSubpass(const std::vector<Subpass>& subpasses) override;
            void AddSubpassDependency(const SubpassDependency& dependency) override {assert(0 && "Not implemented");}

            void TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf, std::vector<RefPtr<VKImage>>& attachments);

            VkRenderPass GetHandle();
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
    };
}
