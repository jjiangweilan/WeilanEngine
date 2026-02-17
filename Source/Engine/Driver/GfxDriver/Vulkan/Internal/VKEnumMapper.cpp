#include "VKEnumMapper.hpp"
#include "Engine/Library/Assert.hpp"
#include <spdlog/spdlog.h>

namespace Gfx
{
VkFormat MapFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::BC3_SRGB_Block: return VK_FORMAT_BC3_SRGB_BLOCK;
        case GfxFormat::BC3_Unorm_Block: return VK_FORMAT_BC3_UNORM_BLOCK;
        case GfxFormat::BC7_SRGB_UNorm_Block: return VK_FORMAT_BC7_SRGB_BLOCK;
        case GfxFormat::BC7_UNorm_Block: return VK_FORMAT_BC7_UNORM_BLOCK;
        case GfxFormat::R16G16B16A16_SFloat: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case GfxFormat::R16G16B16A16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
        case GfxFormat::D16_UNorm: return VK_FORMAT_D16_UNORM;
        case GfxFormat::D16_UNorm_S8_UInt: return VK_FORMAT_D16_UNORM_S8_UINT;
        case GfxFormat::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
        case GfxFormat::D32_SFloat: return VK_FORMAT_D32_SFLOAT;
        case GfxFormat::D32_SFLOAT_S8_UInt: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case GfxFormat::B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
        case GfxFormat::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case GfxFormat::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
        case GfxFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case GfxFormat::R8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case GfxFormat::R8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case GfxFormat::R8_SRGB: return VK_FORMAT_R8_SRGB;
        case GfxFormat::R16G16_UNorm: return VK_FORMAT_R16G16_UNORM;
        case GfxFormat::R16G16_SNorm: return VK_FORMAT_R16G16_SNORM;
        case GfxFormat::R16G16_UScaled: return VK_FORMAT_R16G16_USCALED;
        case GfxFormat::R16G16_SScaled: return VK_FORMAT_R16G16_SSCALED;
        case GfxFormat::R16G16_UInt: return VK_FORMAT_R16G16_UINT;
        case GfxFormat::R16G16_SInt: return VK_FORMAT_R16G16_SINT;
        case GfxFormat::R16G16_SFloat: return VK_FORMAT_R16G16_SFLOAT;
        case GfxFormat::R32G32_UInt: return VK_FORMAT_R32G32_UINT;
        case GfxFormat::R32G32_SInt: return VK_FORMAT_R32G32_SINT;
        case GfxFormat::R32G32_SFloat: return VK_FORMAT_R32G32_SFLOAT;
        case GfxFormat::R32G32B32_UInt: return VK_FORMAT_R32G32B32_UINT;
        case GfxFormat::R32G32B32_SInt: return VK_FORMAT_R32G32B32_SINT;
        case GfxFormat::R32G32B32_SFloat: return VK_FORMAT_R32G32B32_SFLOAT;
        case GfxFormat::R32G32B32A32_UInt: return VK_FORMAT_R32G32B32A32_UINT;
        case GfxFormat::R32G32B32A32_SInt: return VK_FORMAT_R32G32B32A32_SINT;
        case GfxFormat::R32G32B32A32_SFloat: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case GfxFormat::R16G16B16_UNorm: return VK_FORMAT_R16G16B16_UNORM;
        case GfxFormat::R16G16B16_SNorm: return VK_FORMAT_R16G16B16_SNORM;
        case GfxFormat::R16G16B16_UScaled: return VK_FORMAT_R16G16B16_USCALED;
        case GfxFormat::R16G16B16_SScaled: return VK_FORMAT_R16G16B16_SSCALED;
        case GfxFormat::R16G16B16_UInt: return VK_FORMAT_R16G16B16_UINT;
        case GfxFormat::R16G16B16_SInt: return VK_FORMAT_R16G16B16_SINT;
        case GfxFormat::R16G16B16_SFloat: return VK_FORMAT_R16G16B16_SFLOAT;
        case GfxFormat::R32_SFloat: return VK_FORMAT_R32_SFLOAT;
        case GfxFormat::R16_SFloat: return VK_FORMAT_R16_SFLOAT;
        case GfxFormat::R16_UNorm: return VK_FORMAT_R16_UNORM;
        case GfxFormat::B10G11R11_UFloat_Pack32: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case GfxFormat::A2B10G10R10_UNorm: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case GfxFormat::R8_UNorm: return VK_FORMAT_R8_UNORM;
        case GfxFormat::R8_UInt: return VK_FORMAT_R8_UINT;
        default: ASSERT(0 && "Format map failed");
    }

    SPDLOG_WARN("VKEnum map failed");
    return VK_FORMAT_R16G16B16A16_SFLOAT;
}

GfxFormat MapVKFormat(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_BC3_UNORM_BLOCK: return GfxFormat::BC3_Unorm_Block;
        case VK_FORMAT_BC3_SRGB_BLOCK: return GfxFormat::BC3_SRGB_Block;
        case VK_FORMAT_BC7_SRGB_BLOCK: return GfxFormat::BC7_SRGB_UNorm_Block;
        case VK_FORMAT_BC7_UNORM_BLOCK: return GfxFormat::BC7_UNorm_Block;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return GfxFormat::R16G16B16A16_SFloat;
        case VK_FORMAT_R16G16B16A16_UNORM: return GfxFormat::R16G16B16A16_UNorm;
        case VK_FORMAT_D16_UNORM: return GfxFormat::D16_UNorm;
        case VK_FORMAT_D16_UNORM_S8_UINT: return GfxFormat::D16_UNorm_S8_UInt;
        case VK_FORMAT_D24_UNORM_S8_UINT: return GfxFormat::D24_UNorm_S8_UInt;
        case VK_FORMAT_D32_SFLOAT: return GfxFormat::D32_SFloat;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return GfxFormat::D32_SFLOAT_S8_UInt;
        case VK_FORMAT_B8G8R8A8_UNORM: return GfxFormat::B8G8R8A8_UNorm;
        case VK_FORMAT_B8G8R8A8_SRGB: return GfxFormat::B8G8R8A8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return GfxFormat::R8G8B8A8_UNorm;
        case VK_FORMAT_R8G8B8A8_SRGB: return GfxFormat::R8G8B8A8_SRGB;
        case VK_FORMAT_R8G8B8_SRGB: return GfxFormat::R8G8B8_SRGB;
        case VK_FORMAT_R8G8_SRGB: return GfxFormat::R8G8_SRGB;
        case VK_FORMAT_R8_SRGB: return GfxFormat::R8_SRGB;
        case VK_FORMAT_R16G16_UNORM: return GfxFormat::R16G16_UNorm;
        case VK_FORMAT_R16G16_SNORM: return GfxFormat::R16G16_SNorm;
        case VK_FORMAT_R16G16_USCALED: return GfxFormat::R16G16_UScaled;
        case VK_FORMAT_R16G16_SSCALED: return GfxFormat::R16G16_SScaled;
        case VK_FORMAT_R16G16_UINT: return GfxFormat::R16G16_UInt;
        case VK_FORMAT_R16G16_SINT: return GfxFormat::R16G16_SInt;
        case VK_FORMAT_R16G16_SFLOAT: return GfxFormat::R16G16_SFloat;
        case VK_FORMAT_R32G32_UINT: return GfxFormat::R32G32_UInt;
        case VK_FORMAT_R32G32_SINT: return GfxFormat::R32G32_SInt;
        case VK_FORMAT_R32G32_SFLOAT: return GfxFormat::R32G32_SFloat;
        case VK_FORMAT_R32G32B32_UINT: return GfxFormat::R32G32B32_UInt;
        case VK_FORMAT_R32G32B32_SINT: return GfxFormat::R32G32B32_SInt;
        case VK_FORMAT_R32G32B32_SFLOAT: return GfxFormat::R32G32B32_SFloat;
        case VK_FORMAT_R32G32B32A32_UINT: return GfxFormat::R32G32B32A32_UInt;
        case VK_FORMAT_R32G32B32A32_SINT: return GfxFormat::R32G32B32A32_SInt;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return GfxFormat::R32G32B32A32_SFloat;
        case VK_FORMAT_R16G16B16_UNORM: return GfxFormat::R16G16B16_UNorm;
        case VK_FORMAT_R16G16B16_SNORM: return GfxFormat::R16G16B16_SNorm;
        case VK_FORMAT_R16G16B16_USCALED: return GfxFormat::R16G16B16_UScaled;
        case VK_FORMAT_R16G16B16_SSCALED: return GfxFormat::R16G16B16_SScaled;
        case VK_FORMAT_R16G16B16_UINT: return GfxFormat::R16G16B16_UInt;
        case VK_FORMAT_R16G16B16_SINT: return GfxFormat::R16G16B16_SInt;
        case VK_FORMAT_R16G16B16_SFLOAT: return GfxFormat::R16G16B16_SFloat;
        case VK_FORMAT_R32_SFLOAT: return GfxFormat::R32_SFloat;
        case VK_FORMAT_R16_SFLOAT: return GfxFormat::R16_SFloat;
        case VK_FORMAT_R16_UNORM: return GfxFormat::R16_UNorm;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return GfxFormat::B10G11R11_UFloat_Pack32;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return GfxFormat::A2B10G10R10_UNorm;
        case VK_FORMAT_R8_UNORM: return GfxFormat::R8_UNorm;
        case VK_FORMAT_R8_UINT: return GfxFormat::R8_UInt;
        default: ASSERT(0 && "VK format map failed");
    }

    return GfxFormat::R16G16B16A16_SFloat;
}

VkAttachmentLoadOp MapAttachmentLoadOp(AttachmentLoadOperation loadOp)
{
    switch (loadOp)
    {
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
    switch (storeOp)
    {
        case AttachmentStoreOperation::Store: return VK_ATTACHMENT_STORE_OP_STORE;
        case AttachmentStoreOperation::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
        default: return VK_ATTACHMENT_STORE_OP_STORE;
    }

    SPDLOG_WARN("VKEnum map failed");
    return VK_ATTACHMENT_STORE_OP_STORE;
}

VkSampleCountFlagBits MapSampleCount(MultiSampling multiSampling)
{
    switch (multiSampling)
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
    switch (cullMode)
    {
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
    switch (cmp)
    {
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
    switch (op)
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
    if (in & ImageUsage::ColorAttachment)
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (in & ImageUsage::DepthStencilAttachment)
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (in & ImageUsage::Texture)
        flags |= (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    if (in & ImageUsage::TransferSrc)
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (in & ImageUsage::TransferDst)
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (in & ImageUsage::Storage)
        flags |=
            (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

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

VkPipelineStageFlags MapPipelineStage(PipelineStageFlags stages)
{
    VkPipelineStageFlags flags = 0;

    if (HasFlag(stages, PipelineStage::Top_Of_Pipe))
    {
        flags |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    if (HasFlag(stages, PipelineStage::Draw_Indirect))
    {
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    }
    if (HasFlag(stages, PipelineStage::Vertex_Input))
    {
        flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    if (HasFlag(stages, PipelineStage::Vertex_Shader))
    {
        flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Tessellation_Control_Shader))
    {
        flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Tessellation_Evaluation_Shader))
    {
        flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Geometry_Shader))
    {
        flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Fragment_Shader))
    {
        flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Early_Fragment_Tests))
    {
        flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    if (HasFlag(stages, PipelineStage::Late_Fragment_Tests))
    {
        flags |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }
    if (HasFlag(stages, PipelineStage::Color_Attachment_Output))
    {
        flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    if (HasFlag(stages, PipelineStage::Compute_Shader))
    {
        flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Transfer))
    {
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    if (HasFlag(stages, PipelineStage::Bottom_Of_Pipe))
    {
        flags |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    if (HasFlag(stages, PipelineStage::Host))
    {
        flags |= VK_PIPELINE_STAGE_HOST_BIT;
    }
    if (HasFlag(stages, PipelineStage::All_Graphics))
    {
        flags |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    if (HasFlag(stages, PipelineStage::All_Commands))
    {
        flags |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    return flags;
}

VkAccessFlags MapAccessMask(AccessMaskFlags masks)
{
    VkAccessFlags flags = 0;

    if (HasFlag(masks, AccessMask::Indirect_Command_Read))
    {
        flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Index_Read))
    {
        flags |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Vertex_Attribute_Read))
    {
        flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Uniform_Read))
    {
        flags |= VK_ACCESS_UNIFORM_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Input_Attachment_Read))
    {
        flags |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Shader_Read))
    {
        flags |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Shader_Write))
    {
        flags |= VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (HasFlag(masks, AccessMask::Color_Attachment_Read))
    {
        flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Color_Attachment_Write))
    {
        flags |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (HasFlag(masks, AccessMask::Depth_Stencil_Attachment_Read))
    {
        flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Depth_Stencil_Attachment_Write))
    {
        flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (HasFlag(masks, AccessMask::Transfer_Read))
    {
        flags |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Transfer_Write))
    {
        flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (HasFlag(masks, AccessMask::Host_Read))
    {
        flags |= VK_ACCESS_HOST_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Host_Write))
    {
        flags |= VK_ACCESS_HOST_WRITE_BIT;
    }
    if (HasFlag(masks, AccessMask::Memory_Read))
    {
        flags |= VK_ACCESS_MEMORY_READ_BIT;
    }
    if (HasFlag(masks, AccessMask::Memory_Write))
    {
        flags |= VK_ACCESS_MEMORY_WRITE_BIT;
    }

    return flags;
}

VkImageLayout MapImageLayout(ImageLayout layout)
{
    if (layout == ImageLayout::Undefined)
        return VK_IMAGE_LAYOUT_UNDEFINED;
    if (layout == ImageLayout::General)
        return VK_IMAGE_LAYOUT_GENERAL;
    if (layout == ImageLayout::Color_Attachment)
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (layout == ImageLayout::Depth_Stencil_Attachment)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (layout == ImageLayout::Depth_Stencil_Read_Only)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (layout == ImageLayout::Shader_Read_Only)
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (layout == ImageLayout::Transfer_Src)
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (layout == ImageLayout::Transfer_Dst)
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (layout == ImageLayout::Preinitialized)
        return VK_IMAGE_LAYOUT_PREINITIALIZED;

    if (layout == ImageLayout::Depth_Read_Only_Stencil_Attachment)
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL;
    if (layout == ImageLayout::Depth_Attachment_Stencil_Read_Only)
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL;
    if (layout == ImageLayout::Depth_Attachment)
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    if (layout == ImageLayout::Depth_Read_Only)
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    if (layout == ImageLayout::Stencil_Attachment)
        return VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;
    if (layout == ImageLayout::Stencil_Read_Only)
        return VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    if (layout == ImageLayout::Read_Only)
        return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR;
    if (layout == ImageLayout::Attachment)
        return VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
    if (layout == ImageLayout::Present_Src_Khr)
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return VK_IMAGE_LAYOUT_GENERAL;
}

ImageAspectFlags MapVKImageAspect(VkImageAspectFlags aspects)
{
    ImageAspectFlags flags = ImageAspectFlags::None;

    if (aspects & VK_IMAGE_ASPECT_COLOR_BIT)
        flags |= ImageAspectFlags::Color;
    if (aspects & VK_IMAGE_ASPECT_DEPTH_BIT)
        flags |= ImageAspectFlags::Depth;
    if (aspects & VK_IMAGE_ASPECT_STENCIL_BIT)
        flags |= ImageAspectFlags::Stencil;
    if (aspects & VK_IMAGE_ASPECT_METADATA_BIT)
        flags |= ImageAspectFlags::Metadata;
    if (aspects & VK_IMAGE_ASPECT_PLANE_0_BIT)
        flags |= ImageAspectFlags::Memory_Plane_0;
    if (aspects & VK_IMAGE_ASPECT_PLANE_1_BIT)
        flags |= ImageAspectFlags::Memory_Plane_1;
    if (aspects & VK_IMAGE_ASPECT_PLANE_2_BIT)
        flags |= ImageAspectFlags::Memory_Plane_2;

    return flags;
}

VkImageAspectFlags MapImageAspect(ImageAspectFlags aspects)
{
    VkImageAspectFlags flags = 0;

    if (HasFlag(aspects, ImageAspect::Color))
    {
        flags |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Depth))
    {
        flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Stencil))
    {
        flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Metadata))
    {
        flags |= VK_IMAGE_ASPECT_METADATA_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Memory_Plane_0))
    {
        flags |= VK_IMAGE_ASPECT_PLANE_0_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Memory_Plane_1))
    {
        flags |= VK_IMAGE_ASPECT_PLANE_1_BIT;
    }
    if (HasFlag(aspects, ImageAspect::Memory_Plane_2))
    {
        flags |= VK_IMAGE_ASPECT_PLANE_2_BIT;
    }

    return flags;
}

ImageLayout MapVKImageLayout(VkImageLayout layout)
{
    if (layout == VK_IMAGE_LAYOUT_UNDEFINED)
        return ImageLayout::Undefined;
    if (layout == VK_IMAGE_LAYOUT_GENERAL)
        return ImageLayout::General;
    if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        return ImageLayout::Color_Attachment;
    if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        return ImageLayout::Depth_Stencil_Attachment;
    if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        return ImageLayout::Depth_Stencil_Read_Only;
    if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return ImageLayout::Shader_Read_Only;
    if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        return ImageLayout::Transfer_Src;
    if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        return ImageLayout::Transfer_Dst;
    if (layout == VK_IMAGE_LAYOUT_PREINITIALIZED)
        return ImageLayout::Preinitialized;

    if (layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL)
        return ImageLayout::Depth_Read_Only_Stencil_Attachment;
    if (layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL)
        return ImageLayout::Depth_Attachment_Stencil_Read_Only;
    if (layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
        return ImageLayout::Depth_Attachment;
    if (layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL)
        return ImageLayout::Depth_Read_Only;
    if (layout == VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL)
        return ImageLayout::Stencil_Attachment;
    if (layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL)
        return ImageLayout::Stencil_Read_Only;
    if (layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR)
        return ImageLayout::Read_Only;
    if (layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR)
        return ImageLayout::Attachment;
    if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        return ImageLayout::Present_Src_Khr;

    return ImageLayout::Undefined;
}

VkPrimitiveTopology MapPrimitiveTopology(Topology topology)
{
    switch (topology)
    {
        case Topology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case Topology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case Topology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}
Topology MapVKPrimitiveTopology(VkPrimitiveTopology topology)
{
    switch (topology)
    {
        case (VK_PRIMITIVE_TOPOLOGY_LINE_LIST): return Topology::LineList;
        case (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST): return Topology::TriangleList;
        case (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP): return Topology::TriangleStrip;
        case (VK_PRIMITIVE_TOPOLOGY_LINE_STRIP): return Topology::LineStrip;
    }
    return Topology::TriangleList;
}

VkPolygonMode MapPolygonMode(PolygonMode mode)
{
    switch (mode)
    {
        case (PolygonMode::Line): return VK_POLYGON_MODE_LINE;
        case (PolygonMode::Point): return VK_POLYGON_MODE_POINT;
        case (PolygonMode::Fill): return VK_POLYGON_MODE_FILL;
    }

    return VK_POLYGON_MODE_FILL;
}

PolygonMode MapVKPolygonMode(VkPolygonMode mode)
{
    switch (mode)
    {
        case (VK_POLYGON_MODE_LINE): return PolygonMode::Line;
        case (VK_POLYGON_MODE_POINT): return PolygonMode::Point;
        case (VK_POLYGON_MODE_FILL): return PolygonMode::Fill;
    }

    return PolygonMode::Fill;
}

VkShaderStageFlags MapShaderStages(ShaderStageFlags stages)
{
    VkShaderStageFlags output = 0;
    if (HasFlag(stages, ShaderStage::Vertex))
        output |= VK_SHADER_STAGE_VERTEX_BIT;
    if (HasFlag(stages, ShaderStage::Fragment))
        output |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (HasFlag(stages, ShaderStage::Compute))
        output |= VK_SHADER_STAGE_COMPUTE_BIT;

    return output;
}

VkDescriptorType MapDescriptorType(DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::SampledImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case DescriptorType::StorageImage: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::UniformTexelBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case DescriptorType::StorageTexelBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case DescriptorType::AccelerationStructure: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default: ASSERT(0 && "Map BindingType failed");
    }

    SPDLOG_CRITICAL("failed to map binding type");
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
}

VkSamplerAddressMode MapSamplerAddressMode(SamplerAddressMode mode)
{
    switch (mode)
    {
        case SamplerAddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerAddressMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SamplerAddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerAddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case SamplerAddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }

    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkFilter MapFilter(FilterMode mode)
{
    switch (mode)
    {
        case FilterMode::Linear: return VK_FILTER_LINEAR;
        case FilterMode::Nearest: return VK_FILTER_NEAREST;
    }

    return VK_FILTER_LINEAR;
}
} // namespace Gfx
