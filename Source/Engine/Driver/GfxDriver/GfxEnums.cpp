#include "GfxEnums.hpp"
#include "Engine/Library/Assert.hpp"
namespace Gfx
{
bool IsSRGBFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::B8G8R8A8_SRGB:
        case GfxFormat::R8G8B8A8_SRGB:
        case GfxFormat::R8G8B8_SRGB:
        case GfxFormat::R8G8_SRGB:
        case GfxFormat::R8_SRGB:
        case GfxFormat::BC7_SRGB_UNorm_Block:
        case GfxFormat::BC3_SRGB_Block: return true;
        default: return false;
    }
}

bool HasWriteAccessMask(AccessMaskFlags flags)
{
    if (HasFlag(flags, AccessMask::Shader_Write) || HasFlag(flags, AccessMask::Color_Attachment_Write) ||
        HasFlag(flags, AccessMask::Transfer_Write) || HasFlag(flags, AccessMask::Host_Write) ||
        HasFlag(flags, AccessMask::Memory_Write))
        return true;

    return false;
}

bool HasReadAccessMask(AccessMaskFlags flags)
{
    if (HasFlag(flags, AccessMask::Indirect_Command_Read) || HasFlag(flags, AccessMask::Index_Read) ||
        HasFlag(flags, AccessMask::Vertex_Attribute_Read) || HasFlag(flags, AccessMask::Uniform_Read) ||
        HasFlag(flags, AccessMask::Input_Attachment_Read) || HasFlag(flags, AccessMask::Shader_Read) ||
        HasFlag(flags, AccessMask::Color_Attachment_Read) ||
        HasFlag(flags, AccessMask::Depth_Stencil_Attachment_Read) || HasFlag(flags, AccessMask::Transfer_Read) ||
        HasFlag(flags, AccessMask::Host_Read) || HasFlag(flags, AccessMask::Memory_Read))
        return true;

    return false;
}

GfxFormat MapStringToGfxFormat(std::string_view name)
{
    if (name == "R16G16B16A16_SFloat")
        return GfxFormat::R16G16B16A16_SFloat;
    else if (name == "R16G16B16A16_UNorm")
        return GfxFormat::R16G16B16A16_UNorm;
    else if (name == "R8G8B8A8_UNorm")
        return GfxFormat::R8G8B8A8_UNorm;
    else if (name == "R8G8_UNorm")
        return GfxFormat::R8G8_UNorm;
    else if (name == "B8G8R8A8_UNorm")
        return GfxFormat::B8G8R8A8_UNorm;
    else if (name == "B8G8R8A8_SRGB")
        return GfxFormat::B8G8R8A8_SRGB;
    else if (name == "R8G8B8A8_SRGB")
        return GfxFormat::R8G8B8A8_SRGB;
    else if (name == "R8G8B8_SRGB")
        return GfxFormat::R8G8B8_SRGB;
    else if (name == "R8G8_SRGB")
        return GfxFormat::R8G8_SRGB;
    else if (name == "R8_SRGB")
        return GfxFormat::R8_SRGB;
    else if (name == "R16G16_UNorm")
        return GfxFormat::R16G16_UNorm;
    else if (name == "R16G16_SNorm")
        return GfxFormat::R16G16_SNorm;
    else if (name == "R16G16_UScaled")
        return GfxFormat::R16G16_UScaled;
    else if (name == "R16G16_SScaled")
        return GfxFormat::R16G16_SScaled;
    else if (name == "R16G16_UInt")
        return GfxFormat::R16G16_UInt;
    else if (name == "R16G16_SInt")
        return GfxFormat::R16G16_SInt;
    else if (name == "R16G16_SFloat")
        return GfxFormat::R16G16_SFloat;
    else if (name == "R32G32_UInt")
        return GfxFormat::R32G32_UInt;
    else if (name == "R32G32_SInt")
        return GfxFormat::R32G32_SInt;
    else if (name == "R32G32_SFloat")
        return GfxFormat::R32G32_SFloat;
    else if (name == "R32G32B32_UInt")
        return GfxFormat::R32G32B32_UInt;
    else if (name == "R32G32B32_SInt")
        return GfxFormat::R32G32B32_SInt;
    else if (name == "R32G32B32_SFloat")
        return GfxFormat::R32G32B32_SFloat;
    else if (name == "R32G32B32A32_UInt")
        return GfxFormat::R32G32B32A32_UInt;
    else if (name == "R32G32B32A32_SInt")
        return GfxFormat::R32G32B32A32_SInt;
    else if (name == "R32G32B32A32_SFloat")
        return GfxFormat::R32G32B32A32_SFloat;
    else if (name == "D16_UNorm")
        return GfxFormat::D16_UNorm;
    else if (name == "D16_UNorm_S8_UInt")
        return GfxFormat::D16_UNorm_S8_UInt;
    else if (name == "D32_SFloat")
        return GfxFormat::D32_SFloat;
    else if (name == "D32_SFLOAT_S8_UInt")
        return GfxFormat::D32_SFLOAT_S8_UInt;
    else if (name == "D24_UNorm_S8_UInt")
        return GfxFormat::D24_UNorm_S8_UInt;
    else if (name == "BC7_UNorm_Block")
        return GfxFormat::BC7_UNorm_Block;
    else if (name == "BC7_SRGB_UNorm_Block")
        return GfxFormat::BC7_SRGB_UNorm_Block;
    else if (name == "BC3_Unorm_Block")
        return GfxFormat::BC3_Unorm_Block;
    else if (name == "BC3_SRGB_Block")
        return GfxFormat::BC3_SRGB_Block;
    else if (name == "B10G11R11_UFloat_Pack32")
        return GfxFormat::B10G11R11_UFloat_Pack32;
    else if (name == "A2B10G10R10_UNorm")
        return GfxFormat::A2B10G10R10_UNorm;
    else if (name == "R16G16B16_UNorm")
        return GfxFormat::R16G16B16_UNorm;
    else if (name == "R16G16B16_SNorm")
        return GfxFormat::R16G16B16_SNorm;
    else if (name == "R16G16B16_UScaled")
        return GfxFormat::R16G16B16_UScaled;
    else if (name == "R16G16B16_SScaled")
        return GfxFormat::R16G16B16_SScaled;
    else if (name == "R16G16B16_UInt")
        return GfxFormat::R16G16B16_UInt;
    else if (name == "R16G16B16_SInt")
        return GfxFormat::R16G16B16_SInt;
    else if (name == "R16G16B16_SFloat")
        return GfxFormat::R16G16B16_SFloat;
    else if (name == "R32_UInt")
        return GfxFormat::R32_UInt;
    else if (name == "R8_UNorm")
        return GfxFormat::R8_UNorm;

    return GfxFormat::Invalid;
}

const char* MapGfxFormatToString(GfxFormat format)
{
    if (format == GfxFormat::R16G16B16A16_SFloat)
        return "R16G16B16A16_SFloat";
    else if (format == GfxFormat::R16G16B16A16_UNorm)
        return "R16G16B16A16_UNorm";
    else if (format == GfxFormat::R8G8B8A8_UNorm)
        return "R8G8B8A8_UNorm";
    else if (format == GfxFormat::B8G8R8A8_UNorm)
        return "B8G8R8A8_UNorm";
    else if (format == GfxFormat::B8G8R8A8_SRGB)
        return "B8G8R8A8_SRGB";
    else if (format == GfxFormat::R8G8B8A8_SRGB)
        return "R8G8B8A8_SRGB";
    else if (format == GfxFormat::R8G8B8_SRGB)
        return "R8G8B8_SRGB";
    else if (format == GfxFormat::R8G8_SRGB)
        return "R8G8_SRGB";
    else if (format == GfxFormat::R8_SRGB)
        return "R8_SRGB";
    else if (format == GfxFormat::R16G16_UNorm)
        return "R16G16_UNorm";
    else if (format == GfxFormat::R16G16_SNorm)
        return "R16G16_SNorm";
    else if (format == GfxFormat::R16G16_UScaled)
        return "R16G16_UScaled";
    else if (format == GfxFormat::R16G16_SScaled)
        return "R16G16_SScaled";
    else if (format == GfxFormat::R16G16_UInt)
        return "R16G16_UInt";
    else if (format == GfxFormat::R16G16_SInt)
        return "R16G16_SInt";
    else if (format == GfxFormat::R16G16_SFloat)
        return "R16G16_SFloat";
    else if (format == GfxFormat::R32G32_UInt)
        return "R32G32_UInt";
    else if (format == GfxFormat::R32G32_SInt)
        return "R32G32_SInt";
    else if (format == GfxFormat::R32G32_SFloat)
        return "R32G32_SFloat";
    else if (format == GfxFormat::R32G32B32_UInt)
        return "R32G32B32_UInt";
    else if (format == GfxFormat::R32G32B32_SInt)
        return "R32G32B32_SInt";
    else if (format == GfxFormat::R32G32B32_SFloat)
        return "R32G32B32_SFloat";
    else if (format == GfxFormat::R32G32B32A32_UInt)
        return "R32G32B32A32_UInt";
    else if (format == GfxFormat::R32G32B32A32_SInt)
        return "R32G32B32A32_SInt";
    else if (format == GfxFormat::R32G32B32A32_SFloat)
        return "R32G32B32A32_SFloat";
    else if (format == GfxFormat::R32_SFloat)
        return "R32_SFloat";
    else if (format == GfxFormat::R32_UInt)
        return "R32_UInt";
    else if (format == GfxFormat::D16_UNorm)
        return "D16_UNorm";
    else if (format == GfxFormat::D16_UNorm_S8_UInt)
        return "D16_UNorm_S8_UInt";
    else if (format == GfxFormat::D32_SFloat)
        return "D32_SFloat";
    else if (format == GfxFormat::D32_SFLOAT_S8_UInt)
        return "D32_SFLOAT_S8_UInt";
    else if (format == GfxFormat::D24_UNorm_S8_UInt)
        return "D24_UNorm_S8_UInt";
    else if (format == GfxFormat::BC7_UNorm_Block)
        return "BC7_UNorm_Block";
    else if (format == GfxFormat::BC7_SRGB_UNorm_Block)
        return "BC7_SRGB_UNorm_Block";
    else if (format == GfxFormat::BC3_Unorm_Block)
        return "BC3_Unorm_Block";
    else if (format == GfxFormat::BC3_SRGB_Block)
        return "BC3_SRGB_Block";
    else if (format == GfxFormat::B10G11R11_UFloat_Pack32)
        return "B10G11R11_UFloat_Pack32";
    else if (format == GfxFormat::A2B10G10R10_UNorm)
        return "A2B10G10R10_UNorm";
    else if (format == GfxFormat::R16G16B16_UNorm)
        return "R16G16B16_UNorm";
    else if (format == GfxFormat::R16G16B16_SNorm)
        return "R16G16B16_SNorm";
    else if (format == GfxFormat::R16G16B16_UScaled)
        return "R16G16B16_UScaled";
    else if (format == GfxFormat::R16G16B16_SScaled)
        return "R16G16B16_SScaled";
    else if (format == GfxFormat::R16G16B16_UInt)
        return "R16G16B16_UInt";
    else if (format == GfxFormat::R16G16B16_SInt)
        return "R16G16B16_SInt";
    else if (format == GfxFormat::R16G16B16_SFloat)
        return "R16G16B16_SFloat";
    else if (format == GfxFormat::R8_UNorm)
        return "R8_UNorm";
    else if (format == GfxFormat::R8G8_UNorm)
        return "R8G8_UNorm";
    else if (format == GfxFormat::R8_UInt)
        return "R8_UInt";
    else if (format == GfxFormat::R16_UNorm)
        return "R16_UNorm";
    return "Invalid";
}

// https://registry.khronos.org/vulkan/specs/1.3-khr-extensions/html/chap40.html#formats-definition
uint32_t MapGfxFormatToByteSize(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::B10G11R11_UFloat_Pack32: return 4;
        case GfxFormat::A2B10G10R10_UNorm : return 4;
        case GfxFormat::BC7_UNorm_Block: return 1;
        case GfxFormat::BC7_SRGB_UNorm_Block: return 1;
        case GfxFormat::BC3_Unorm_Block: return 1;
        case GfxFormat::BC3_SRGB_Block: return 1;
        case GfxFormat::R16G16B16A16_SFloat: return 8;
        case GfxFormat::R32G32B32A32_SFloat: return 16;
        case GfxFormat::R16G16B16A16_UNorm: return 8;
        case GfxFormat::R8G8B8A8_UNorm:
        case GfxFormat::B8G8R8A8_UNorm:
        case GfxFormat::B8G8R8A8_SRGB: return 4;
        case GfxFormat::R8G8B8A8_SRGB: return 4;
        case GfxFormat::R8_UNorm: return 1;
        case GfxFormat::R8G8_UNorm: return 2;
        case GfxFormat::R8_UInt: return 1;
        case GfxFormat::R8G8B8_SRGB: return 3;
        case GfxFormat::R8G8_SRGB: return 2;
        case GfxFormat::R8_SRGB: return 1;
        case GfxFormat::R32_SFloat: return 4;
        case GfxFormat::R32_UInt: return 4;
        case GfxFormat::R16_SFloat: return 2;
        case GfxFormat::R16_UNorm: return 2;
        case GfxFormat::R16G16_UNorm: return 4;
        case GfxFormat::R16G16_SNorm: return 4;
        case GfxFormat::R16G16_UScaled: return 4;
        case GfxFormat::R16G16_SScaled: return 4;
        case GfxFormat::R16G16_UInt: return 4;
        case GfxFormat::R16G16_SInt: return 4;
        case GfxFormat::R16G16_SFloat: return 4;
        case GfxFormat::R16G16B16_UNorm: return 6;
        case GfxFormat::R16G16B16_SNorm: return 6;
        case GfxFormat::R16G16B16_UScaled: return 6;
        case GfxFormat::R16G16B16_SScaled: return 6;
        case GfxFormat::R16G16B16_UInt: return 6;
        case GfxFormat::R16G16B16_SInt: return 6;
        case GfxFormat::R16G16B16_SFloat: return 6;
        case GfxFormat::R32G32_UInt: return 8;
        case GfxFormat::R32G32_SInt: return 8;
        case GfxFormat::R32G32_SFloat: return 8;
        case GfxFormat::R32G32B32_UInt: return 12;
        case GfxFormat::R32G32B32_SInt: return 12;
        case GfxFormat::R32G32B32_SFloat: return 12;
        case GfxFormat::R32G32B32A32_UInt: return 16;
        case GfxFormat::R32G32B32A32_SInt: return 16;
        case GfxFormat::D16_UNorm: return 2;
        case GfxFormat::D16_UNorm_S8_UInt: return 3;
        case GfxFormat::D24_UNorm_S8_UInt: return 4;
        case GfxFormat::D32_SFloat: return 4;
        case GfxFormat::D32_SFLOAT_S8_UInt:
            return 5; // 5 ? from vulkan docs: VK_FORMAT_D32_SFLOAT_S8_UINT specifies a two-component format that has 32
            // signed float bits in the depth component and 8 unsigned integer bits in the stencil component.
            // There are optionally 24 bits that are unused.
        default: ASSERT(0 && "Not implemented");
    }

    return 64;
};

bool IsCompressedFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::BC7_UNorm_Block:
        case GfxFormat::BC7_SRGB_UNorm_Block:
        case GfxFormat::BC3_Unorm_Block:
        case GfxFormat::BC3_SRGB_Block: return true;
        default: return false;
    }
}

uint32_t MapGfxFormatToBlockWidth(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::BC7_UNorm_Block:
        case GfxFormat::BC7_SRGB_UNorm_Block:
        case GfxFormat::BC3_Unorm_Block:
        case GfxFormat::BC3_SRGB_Block: return 4;
        default: return 1;
    }
}

uint32_t MapGfxFormatToBlockHeight(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::BC7_UNorm_Block:
        case GfxFormat::BC7_SRGB_UNorm_Block:
        case GfxFormat::BC3_Unorm_Block:
        case GfxFormat::BC3_SRGB_Block: return 4;
        default: return 1;
    }
}

uint32_t MapGfxFormatToBlockByteSize(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::BC7_UNorm_Block:
        case GfxFormat::BC7_SRGB_UNorm_Block:
        case GfxFormat::BC3_Unorm_Block:
        case GfxFormat::BC3_SRGB_Block: return 16;
        default: return MapGfxFormatToByteSize(format);
    }
}

bool HasStencil(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::D16_UNorm_S8_UInt:
        case GfxFormat::D24_UNorm_S8_UInt:
        case GfxFormat::D32_SFLOAT_S8_UInt: return true;
        default: return false;
    }
}

bool IsDepthStencilFormat(GfxFormat format)
{
    switch (format)
    {
        case GfxFormat::D16_UNorm:
        case GfxFormat::D16_UNorm_S8_UInt:
        case GfxFormat::D24_UNorm_S8_UInt:
        case GfxFormat::D32_SFloat:
        case GfxFormat::D32_SFLOAT_S8_UInt: return true;
        default: return false;
    }
}

bool IsColoFormat(GfxFormat format)
{
    return !IsDepthStencilFormat(format);
}

GfxFormat GetGfxFormat(int channelBits, int channels, bool linear)
{
    GfxFormat format = Gfx::GfxFormat::Invalid;
    if (channels == 4)
    {
        if (channelBits == 32 && linear)
            format = Gfx::GfxFormat::R32G32B32A32_SFloat;
        else if (channelBits == 16 && linear)
            format = Gfx::GfxFormat::R16G16B16A16_UNorm;
        else if (channelBits == 8 && !linear)
            format = Gfx::GfxFormat::R8G8B8A8_SRGB;
        else if (channelBits == 8 && linear)
            format = Gfx::GfxFormat::R8G8B8A8_UNorm;
    }
    else if (channels == 3)
    {
        if (channelBits == 32 && linear)
            format = Gfx::GfxFormat::R32G32B32_SFloat;
        else if (channelBits == 16 && linear)
            format = Gfx::GfxFormat::R16G16B16_UNorm;
        else if (channelBits == 8 && !linear)
            format = Gfx::GfxFormat::R8G8B8_SRGB;
        else if (channelBits == 8 && linear)
            format = Gfx::GfxFormat::R8G8B8A8_UNorm;
    }
    else if (channels == 2)
    {
        if (channelBits == 32 && linear)
            format = Gfx::GfxFormat::R32G32_SFloat;
        else if (channelBits == 16 && linear)
            format = Gfx::GfxFormat::R16G16_UNorm;
        else if (channelBits == 8 && !linear)
            format = Gfx::GfxFormat::R8G8_SRGB;
        else if (channelBits == 8 && linear)
            format = Gfx::GfxFormat::R8G8_UNorm;
    }
    else if (channels == 1)
    {
        if (channelBits == 32 && linear)
            format = Gfx::GfxFormat::R32_SFloat;
        else if (channelBits == 16 && linear)
            format = Gfx::GfxFormat::R16_UNorm;
        else if (channelBits == 8 && !linear)
            format = Gfx::GfxFormat::R8_SRGB;
        else if (channelBits == 8 && linear)
            format = Gfx::GfxFormat::R8_UNorm;
    }

    return format;
}

} // namespace Gfx
