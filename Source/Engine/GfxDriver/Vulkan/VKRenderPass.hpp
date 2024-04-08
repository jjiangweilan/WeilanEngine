#pragma once
#include "../RenderPass.hpp"
#include "GfxDriver/Vulkan/VKImage.hpp"
#include "GfxDriver/Vulkan/VKSwapchainImage.hpp"
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKContext;
class VKImage;
struct Subpass
{
    Subpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) : colors(colors), depth(depth) {}
    std::vector<Attachment> colors;
    std::optional<Attachment> depth;
};
class VKRenderPass : public RenderPass
{
public:
    VKRenderPass();
    VKRenderPass(const VKRenderPass& renderPass) = delete;
    VKRenderPass(VKRenderPass&& renderPass) = delete;
    ~VKRenderPass() override;
    void AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) override;
    void ClearSubpass() override
    {
        subpasses.clear();
    }

    // you should first call GetHandle then GetFrameBuffer
    VkFramebuffer GetFrameBuffer();
    VkRenderPass GetHandle();
    Extent2D GetExtent();

    const std::vector<Subpass>& GetSubpesses()
    {
        return subpasses;
    }

protected:
    void CreateRenderPass();
    VkFramebuffer CreateFrameBuffer();

    VkRenderPass renderPass = VK_NULL_HANDLE;

    // when one of the color attachment is a swap chain image proxy there will be multiple framebuffers, otherwise there
    // is only one
    std::vector<VkFramebuffer> frameBuffers;
    VKSwapChainImage* swapChainProxy = nullptr;
    UUID swapChainProxyUUIDCopy;
    Extent2D extent;
    std::vector<Subpass> subpasses;
};
} // namespace Gfx
