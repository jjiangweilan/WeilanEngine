#pragma once
#include "Engine/Runtime/Object/Graphics/RenderTarget.hpp"
#include "VKImage.hpp"

#include <memory>
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
namespace Gfx
{
class VKMemAllocator;
class VKObjectManager;
class VKAppWindow;
class VKContext;

class VKRenderTarget : public RenderTarget
{
public:
    VKRenderTarget(VKContext* context, VKAppWindow* window, const RenderTargetDescription& renderTargetDescription);
    VKRenderTarget(const VKRenderTarget& other) = delete;
    virtual ~VKRenderTarget() override;

    VkRenderPass RequestRenderPass(const RenderPassConfig& config);

    VkFramebuffer GetVkFrameBuffer();

    const std::vector<VkClearValue>& GetClearValues();
    const VkExtent2D& GetSize();
    void SetRenderTargetDescription(const RenderTargetDescription& renderTargetDescription) override;
    VKImage& GetImage(uint32_t index);

private:
    struct RenderPassStorage
    {
        RenderPassConfig config;
        VkRenderPass renderPass;
    };
    std::vector<RenderPassStorage> renderPasses;

    VkDevice device_vk;
    VkFramebuffer framebuffer_vk = VK_NULL_HANDLE;

    VKMemAllocator* memAllocator;
    VKAppWindow* window;
    VKObjectManager* objectManager;
    std::vector<VKImage> attachments;
    std::vector<VkImageView> attachmentImageViews;
    std::vector<VkClearValue> clearValues;
    VKContext* context;
    VkExtent2D resolution;
    friend class VKDriver;
};

} // namespace Gfx
