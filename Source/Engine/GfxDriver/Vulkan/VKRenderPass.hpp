#pragma once
#include "../RenderPass.hpp"
#include "GfxDriver/Vulkan/VKImage.hpp"
#include <optional>
#include <vector>
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
    void ClearSubpass() override { subpasses.clear(); }

    void TransformAttachmentIfNeeded(VkCommandBuffer cmdBuf);

    VkFramebuffer GetFrameBuffer();
    VkRenderPass GetHandle();
    Extent2D GetExtent();

protected:
    void CreateRenderPass();
    VkFramebuffer CreateFrameBuffer();

    VkRenderPass renderPass = VK_NULL_HANDLE;

    // when one of the color attachment is a swap chain image proxy there will be multiple framebuffers, otherwise there
    // is only one
    std::vector<VkFramebuffer> frameBuffers;
    VKSwapChainImageProxy* swapChainProxy;

    struct Subpass
    {
        Subpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) : colors(colors), depth(depth)
        {}
        std::vector<Attachment> colors;
        std::optional<Attachment> depth;
    };
    std::vector<Subpass> subpasses;
};
} // namespace Engine::Gfx
