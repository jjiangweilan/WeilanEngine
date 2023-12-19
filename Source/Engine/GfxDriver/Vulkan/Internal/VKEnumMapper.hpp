#pragma once

#include "../../GfxEnums.hpp"
#include <vulkan/vulkan.h>
namespace Gfx
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
VkPipelineStageFlags MapPipelineStage(PipelineStageFlags stages);
VkAccessFlags MapAccessMask(AccessMaskFlags masks);
VkImageLayout MapImageLayout(ImageLayout layout);
ImageLayout MapVKImageLayout(VkImageLayout layout);
VkImageAspectFlags MapImageAspect(ImageAspectFlags aspects);
ImageAspectFlags MapVKImageAspect(VkImageAspectFlags aspects);
} // namespace Gfx
