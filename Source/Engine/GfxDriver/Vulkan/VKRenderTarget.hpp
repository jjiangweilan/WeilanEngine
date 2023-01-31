#pragma once
#include "Core/Graphics/RenderTarget.hpp"
#include "VKImage.hpp"

#include <memory>
#include <vector>
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
namespace Engine::Gfx
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

    void TransformAttachmentLayoutIfNeeded(VkCommandBuffer cmd);

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

} // namespace Engine::Gfx
