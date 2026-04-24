#pragma once
#include "../../GfxEnums.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/VKCommon.hpp"
namespace Gfx
{

VkPolygonMode MapPolygonMode(PolygonMode mode);
PolygonMode MapVKPolygonMode(VkPolygonMode mode);
VkPrimitiveTopology MapPrimitiveTopology(Topology topology);
Topology MapVKPrimitiveTopology(VkPrimitiveTopology topology);
VkFormat MapFormat(GfxFormat format);
GfxFormat MapVKFormat(VkFormat format);
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
VkShaderStageFlags MapShaderStages(ShaderStageFlags stages);
VkDescriptorType MapDescriptorType(DescriptorType type);
VkSamplerAddressMode MapSamplerAddressMode(SamplerAddressMode mode);
VkFilter MapFilter(FilterMode mode);
} // namespace Gfx
