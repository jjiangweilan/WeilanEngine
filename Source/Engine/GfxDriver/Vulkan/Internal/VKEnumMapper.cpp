#include "VKEnumMapper.hpp"
#include <spdlog/spdlog.h>

namespace Engine::Gfx::VKEnumMapper
{
    VkFormat MapFormat(ImageFormat format)
    {
        switch(format)
        {
            case ImageFormat::R16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::D16_UNorm: return VK_FORMAT_D16_UNORM;
            case ImageFormat::D16_UNorm_S8_UInt: return VK_FORMAT_D16_UNORM_S8_UINT;
            case ImageFormat::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
            case ImageFormat::B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
            case ImageFormat::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
            case ImageFormat::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
            case ImageFormat::R8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
            case ImageFormat::R8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
            case ImageFormat::R8_SRGB: return VK_FORMAT_R8_SRGB;
            default: assert(0 && "Format map failed");
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    ImageFormat MapVKFormat(VkFormat format)
    {
        switch(format)
        {
            case VK_FORMAT_R16G16B16A16_SFLOAT: return ImageFormat::R16G16B16A16_SFloat;
            case VK_FORMAT_D16_UNORM: return ImageFormat::D16_UNorm;
            case VK_FORMAT_D16_UNORM_S8_UINT: return ImageFormat::D16_UNorm_S8_UInt;
            case VK_FORMAT_D24_UNORM_S8_UINT : return ImageFormat::D24_UNorm_S8_UInt;
            case VK_FORMAT_B8G8R8A8_UNORM: return ImageFormat::B8G8R8A8_UNorm;
            case VK_FORMAT_B8G8R8A8_SRGB: return ImageFormat::B8G8R8A8_SRGB;
            case VK_FORMAT_R8G8B8A8_UNORM: return ImageFormat::R8G8B8A8_UNorm;
            case VK_FORMAT_R8G8B8A8_SRGB: return ImageFormat::R8G8B8A8_SRGB;
            case VK_FORMAT_R8G8B8_SRGB: return ImageFormat::R8G8B8_SRGB;
            case VK_FORMAT_R8G8_SRGB: return ImageFormat::R8G8_SRGB;
            case VK_FORMAT_R8_SRGB: return ImageFormat::R8_SRGB;
            default: assert(0 && "VK format map failed");
        }

        return ImageFormat::R16G16B16A16_SFloat;
    }

    VkAttachmentLoadOp MapAttachmentLoadOp(AttachmentLoadOperation loadOp)
    {
        switch (loadOp) {
            case AttachmentLoadOperation::Load: return VK_ATTACHMENT_LOAD_OP_LOAD;
            case AttachmentLoadOperation::Clear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
            case AttachmentLoadOperation::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            default: return VK_ATTACHMENT_LOAD_OP_LOAD;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    VkAttachmentStoreOp MapAttachmentStoreOp(AttachmentStoreOperation storeOp)
    {
        switch (storeOp) {
            case AttachmentStoreOperation::Store: return VK_ATTACHMENT_STORE_OP_STORE;
            case AttachmentStoreOperation::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            default: return VK_ATTACHMENT_STORE_OP_STORE;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_ATTACHMENT_STORE_OP_STORE;
    }

    VkSampleCountFlagBits MapSampleCount(MultiSampling multiSampling)
    {
        switch(multiSampling)
        {
            case MultiSampling::Sample_Count_1: return VK_SAMPLE_COUNT_1_BIT;
            case MultiSampling::Sample_Count_2: return VK_SAMPLE_COUNT_2_BIT;
            case MultiSampling::Sample_Count_4: return VK_SAMPLE_COUNT_4_BIT;
            case MultiSampling::Sample_Count_8: return VK_SAMPLE_COUNT_8_BIT;
            case MultiSampling::Sample_Count_16: return VK_SAMPLE_COUNT_16_BIT;
            case MultiSampling::Sample_Count_32: return VK_SAMPLE_COUNT_32_BIT;
            case MultiSampling::Sample_Count_64: return VK_SAMPLE_COUNT_64_BIT;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkCullModeFlags MapCullMode(CullMode cullMode)
    {
        switch (cullMode) {
            case CullMode::None: return VK_CULL_MODE_NONE;
            case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::Both: return VK_CULL_MODE_FRONT_AND_BACK;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_CULL_MODE_NONE;
    }

    VkCompareOp MapCompareOp(CompareOp cmp)
    {
        switch (cmp) {
            case CompareOp::Never: return VK_COMPARE_OP_NEVER;
            case CompareOp::Less: return VK_COMPARE_OP_LESS;
            case CompareOp::Less_or_Equal: return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareOp::Greater: return VK_COMPARE_OP_GREATER;
            case CompareOp::Not_Equal: return VK_COMPARE_OP_NOT_EQUAL;
            case CompareOp::Greater_Or_Equal: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
            case CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_COMPARE_OP_ALWAYS;
    }

    VkStencilOp MapStencilOp(StencilOp op)
    {
        switch(op)
        {
            case StencilOp::Keep: return VK_STENCIL_OP_KEEP;
            case StencilOp::Zero: return VK_STENCIL_OP_ZERO;
            case StencilOp::Replace: return VK_STENCIL_OP_REPLACE;
            case StencilOp::Increment_And_Clamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
            case StencilOp::Decrement_And_Clamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
            case StencilOp::Invert: return VK_STENCIL_OP_INVERT;
            case StencilOp::Increment_And_Wrap: return VK_STENCIL_OP_INCREMENT_AND_WRAP;
            case StencilOp::Decrement_And_Wrap: return VK_STENCIL_OP_DECREMENT_AND_WRAP;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_STENCIL_OP_KEEP;
    }

    VkBlendFactor MapBlendFactor(BlendFactor bf)
    {
        switch (bf)
        {
            case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
            case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
            case BlendFactor::Src_Color: return VK_BLEND_FACTOR_SRC_COLOR;
            case BlendFactor::One_Minus_Src_Color: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
            case BlendFactor::Dst_Color: return VK_BLEND_FACTOR_DST_COLOR;
            case BlendFactor::One_Minus_Dst_Color: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
            case BlendFactor::Src_Alpha: return VK_BLEND_FACTOR_SRC_ALPHA;
            case BlendFactor::One_Minus_Src_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::Dst_Alpha: return VK_BLEND_FACTOR_DST_ALPHA;
            case BlendFactor::One_Minus_Dst_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
            case BlendFactor::Constant_Color: return VK_BLEND_FACTOR_CONSTANT_COLOR;
            case BlendFactor::One_Minus_Constant_Color: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
            case BlendFactor::Constant_Alpha: return VK_BLEND_FACTOR_CONSTANT_ALPHA;
            case BlendFactor::One_Minus_Constant_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
            case BlendFactor::Src_Alpha_Saturate: return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
            case BlendFactor::Src1_Color: return VK_BLEND_FACTOR_SRC1_COLOR;
            case BlendFactor::One_Minus_Src1_Color: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
            case BlendFactor::Src1_Alpha: return VK_BLEND_FACTOR_SRC1_ALPHA;
            case BlendFactor::One_Minus_Src1_Alpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_BLEND_FACTOR_ZERO;
    }

    VkImageUsageFlags MapImageUsage(ImageUsageFlags in)
    {
        VkImageUsageFlags flags = 0;
        if (in & ImageUsage::ColorAttachment) flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (in & ImageUsage::DepthStencilAttachment) flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (in & ImageUsage::Texture) flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (in & ImageUsage::TransferSrc) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (in & ImageUsage::TransferDst) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        return flags;
    }

    VkBlendOp MapBlendOp(BlendOp op)
    {
        switch (op)
        {
            case BlendOp::Add: return VK_BLEND_OP_ADD;
            case BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
            case BlendOp::Reverse_Subtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
            case BlendOp::Min: return VK_BLEND_OP_MIN;
            case BlendOp::Max: return VK_BLEND_OP_MAX;
        }

        SPDLOG_WARN("VKEnum map failed");
        return VK_BLEND_OP_ADD;
    }

    VkColorComponentFlagBits MapColorComponentBits(ColorComponentBits bits)
    {
        return (VkColorComponentFlagBits)bits;

        SPDLOG_WARN("VKEnum map failed");
        return VK_COLOR_COMPONENT_R_BIT;
    }
}
