#pragma once

#include "../../GfxEnums.hpp"
#include <vulkan/vulkan.h>
namespace Engine::Gfx::VKEnumMapper
{
    VkFormat MapFormat(ImageFormat format);
    ImageFormat MapVKFormat(VkFormat format);
    VkAttachmentLoadOp MapAttachmentLoadOp(AttachmentLoadOperation loadOp);
    VkAttachmentStoreOp MapAttachmentStoreOp(AttachmentStoreOperation storeOp);
    VkSampleCountFlagBits MapSampleCount(MultiSampling multiSampling);
    VkCullModeFlags MapCullMode(CullMode cullMode);
    VkCompareOp MapCompareOp(CompareOp cmp);
    VkStencilOp MapStencilOp(StencilOp op);
    VkBlendFactor MapBlendFactor(BlendFactor bf);
    VkBlendOp MapBlendOp(BlendOp op);
    VkColorComponentFlagBits MapColorComponentBits(ColorComponentBits bits);
    VkImageUsageFlags MapImageUsage(ImageUsageFlags in);

}
