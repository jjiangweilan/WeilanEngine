#pragma once
#include "../RenderPass.hpp"
#include "GfxDriver/Vulkan/VKImage.hpp"
#include "GfxDriver/Vulkan/VKSwapchainImage.hpp"
#include <optional>
#include "Libs/DynamicArray.hpp"
#include <vulkan/vulkan.h>

namespace Gfx
{
class VKContext;
class VKImage;
struct VKSubpass
{
    VKSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) : colors(colors), depth(depth) {}
    std::vector<Attachment> colors;
    std::optional<Attachment> depth;
};
class VKRenderPass : public RenderPass_Deprecated
{
    DECLARE_OBJECT();

public:
    VKRenderPass();
    VKRenderPass(const VKRenderPass& renderPass) = delete;
    VKRenderPass(VKRenderPass&& renderPass) = delete;
    ~VKRenderPass() override;
    void AddSubpass(const std::vector<Attachment>& colors, std::optional<Attachment> depth) override;
    void ClearSubpass() override { subpasses.clear(); }

    bool RenderPassRenderingValidationCheck() override;

    // you should first call GetHandle then GetFrameBuffer
    VkFramebuffer GetFrameBuffer();
    VkRenderPass GetHandle();
    Extent2D GetExtent();

    const std::vector<VKSubpass>& GetSubpesses() { return subpasses; }

protected:
    void CreateRenderPass();
    VkFramebuffer CreateFrameBuffer();

    VkRenderPass renderPass = VK_NULL_HANDLE;

    // when one of the color attachment is a swap chain image proxy there will be multiple framebuffers, otherwise there
    // is only one
    std::vector<VkFramebuffer> frameBuffers;
    VKSwapChainImage* swapChainProxy = nullptr;
    UUID swapChainProxyUUIDCopy;
    Extent2D extent = {0, 0};
    std::vector<VKSubpass> subpasses;
};
} // namespace Gfx
