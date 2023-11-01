#include "GfxEnums.hpp"

namespace Engine::Gfx
{
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


ImageFormat MapStringToImageFormat(std::string_view name)
{
    if (name == "R16G16B16A16_SFloat")
        return ImageFormat::R16G16B16A16_SFloat;
    else if (name == "R16G16B16A16_UNorm")
        return ImageFormat::R16G16B16A16_UNorm;
    else if (name == "R8G8B8A8_UNorm")
        return ImageFormat::R8G8B8A8_UNorm;
    else if (name == "B8G8R8A8_UNorm")
        return ImageFormat::B8G8R8A8_UNorm;
    else if (name == "B8G8R8A8_SRGB")
        return ImageFormat::B8G8R8A8_SRGB;
    else if (name == "R8G8B8A8_SRGB")
        return ImageFormat::R8G8B8A8_SRGB;
    else if (name == "R8G8B8_SRGB")
        return ImageFormat::R8G8B8_SRGB;
    else if (name == "R8G8_SRGB")
        return ImageFormat::R8G8_SRGB;
    else if (name == "R8_SRGB")
        return ImageFormat::R8_SRGB;
    else if (name == "R16G16_SNorm")
        return ImageFormat::R16G16_SNorm;
    else if (name == "R16G16_UScaled")
        return ImageFormat::R16G16_UScaled;
    else if (name == "R16G16_SScaled")
        return ImageFormat::R16G16_SScaled;
    else if (name == "R16G16_UInt")
        return ImageFormat::R16G16_UInt;
    else if (name == "R16G16_SInt")
        return ImageFormat::R16G16_SInt;
    else if (name == "R16G16_SFloat")
        return ImageFormat::R16G16_SFloat;
    else if (name == "R32G32_UInt")
        return ImageFormat::R32G32_UInt;
    else if (name == "R32G32_SInt")
        return ImageFormat::R32G32_SInt;
    else if (name == "R32G32_SFloat")
        return ImageFormat::R32G32_SFloat;
    else if (name == "R32G32B32_UInt")
        return ImageFormat::R32G32B32_UInt;
    else if (name == "R32G32B32_SInt")
        return ImageFormat::R32G32B32_SInt;
    else if (name == "R32G32B32_SFloat")
        return ImageFormat::R32G32B32_SFloat;
    else if (name == "R32G32B32A32_UInt")
        return ImageFormat::R32G32B32A32_UInt;
    else if (name == "R32G32B32A32_SInt")
        return ImageFormat::R32G32B32A32_SInt;
    else if (name == "R32G32B32A32_SFloat")
        return ImageFormat::R32G32B32A32_SFloat;
    else if (name == "D16_UNorm")
        return ImageFormat::D16_UNorm;
    else if (name == "D16_UNorm_S8_UInt")
        return ImageFormat::D16_UNorm_S8_UInt;
    else if (name == "D32_SFLOAT_S8_UInt")
        return ImageFormat::D32_SFLOAT_S8_UInt;
    else if (name == "D24_UNorm_S8_UInt")
        return ImageFormat::D24_UNorm_S8_UInt;
    else if (name == "BC7_UNorm_Block")
        return ImageFormat::BC7_UNorm_Block;
    else if (name == "BC7_SRGB_UNorm_Block")
        return ImageFormat::BC7_SRGB_UNorm_Block;
    else if (name == "BC3_Unorm_Block")
        return ImageFormat::BC3_Unorm_Block;
    else if (name == "BC3_SRGB_Block")
        return ImageFormat::BC3_SRGB_Block;

    return ImageFormat::Invalid;
}

const char* MapImageFormatToString(ImageFormat format)
{
    if (format == ImageFormat::R16G16B16A16_SFloat)
        return "R16G16B16A16_SFloat";
    else if (format == ImageFormat::R16G16B16A16_UNorm)
        return "R16G16B16A16_UNorm";
    else if (format == ImageFormat::R8G8B8A8_UNorm)
        return "R8G8B8A8_UNorm";
    else if (format == ImageFormat::B8G8R8A8_UNorm)
        return "B8G8R8A8_UNorm";
    else if (format == ImageFormat::B8G8R8A8_SRGB)
        return "B8G8R8A8_SRGB";
    else if (format == ImageFormat::R8G8B8A8_SRGB)
        return "R8G8B8A8_SRGB";
    else if (format == ImageFormat::R8G8B8_SRGB)
        return "R8G8B8_SRGB";
    else if (format == ImageFormat::R8G8_SRGB)
        return "R8G8_SRGB";
    else if (format == ImageFormat::R8_SRGB)
        return "R8_SRGB";
    else if (format == ImageFormat::R16G16_SNorm)
        return "R16G16_SNorm";
    else if (format == ImageFormat::R16G16_UScaled)
        return "R16G16_UScaled";
    else if (format == ImageFormat::R16G16_SScaled)
        return "R16G16_SScaled";
    else if (format == ImageFormat::R16G16_UInt)
        return "R16G16_UInt";
    else if (format == ImageFormat::R16G16_SInt)
        return "R16G16_SInt";
    else if (format == ImageFormat::R16G16_SFloat)
        return "R16G16_SFloat";
    else if (format == ImageFormat::R32G32_UInt)
        return "R32G32_UInt";
    else if (format == ImageFormat::R32G32_SInt)
        return "R32G32_SInt";
    else if (format == ImageFormat::R32G32_SFloat)
        return "R32G32_SFloat";
    else if (format == ImageFormat::R32G32B32_UInt)
        return "R32G32B32_UInt";
    else if (format == ImageFormat::R32G32B32_SInt)
        return "R32G32B32_SInt";
    else if (format == ImageFormat::R32G32B32_SFloat)
        return "R32G32B32_SFloat";
    else if (format == ImageFormat::R32G32B32A32_UInt)
        return "R32G32B32A32_UInt";
    else if (format == ImageFormat::R32G32B32A32_SInt)
        return "R32G32B32A32_SInt";
    else if (format == ImageFormat::R32G32B32A32_SFloat)
        return "R32G32B32A32_SFloat";
    else if (format == ImageFormat::D16_UNorm)
        return "D16_UNorm";
    else if (format == ImageFormat::D16_UNorm_S8_UInt)
        return "D16_UNorm_S8_UInt";
    else if (format == ImageFormat::D32_SFLOAT_S8_UInt)
        return "D32_SFLOAT_S8_UInt";
    else if (format == ImageFormat::D24_UNorm_S8_UInt)
        return "D24_UNorm_S8_UInt";
    else if (format == ImageFormat::BC7_UNorm_Block)
        return "BC7_UNorm_Block";
    else if (format == ImageFormat::BC7_SRGB_UNorm_Block)
        return "BC7_SRGB_UNorm_Block";
    else if (format == ImageFormat::BC3_Unorm_Block)
        return "BC3_Unorm_Block";
    else if (format == ImageFormat::BC3_SRGB_Block)
        return "BC3_SRGB_Block";

    return "Invalid";
}

} // namespace Engine::Gfx

namespace Engine::Gfx::Utils
{

// https://registry.khronos.org/vulkan/specs/1.3-khr-extensions/html/chap40.html#formats-definition
uint32_t MapImageFormatToByteSize(ImageFormat format)
{
    switch (format)
    {
        case ImageFormat::BC7_UNorm_Block: return 16;
        case ImageFormat::BC7_SRGB_UNorm_Block: return 16;
        case ImageFormat::BC3_Unorm_Block: return 16;
        case ImageFormat::BC3_SRGB_Block: return 16;
        case ImageFormat::R16G16B16A16_SFloat: return 8;
        case ImageFormat::R32G32B32A32_SFloat: return 16;
        case ImageFormat::R16G16B16A16_UNorm: return 8;
        case ImageFormat::R8G8B8A8_UNorm:
        case ImageFormat::B8G8R8A8_UNorm:
        case ImageFormat::B8G8R8A8_SRGB: return 4;
        case ImageFormat::R8G8B8A8_SRGB: return 4;
        case ImageFormat::R8G8B8_SRGB: return 3;
        case ImageFormat::R8G8_SRGB: return 2;
        case ImageFormat::R8_SRGB: return 1;
        case ImageFormat::R16G16_UNorm: return 4;
        case ImageFormat::R16G16_SNorm: return 4;
        case ImageFormat::R16G16_UScaled: return 4;
        case ImageFormat::R16G16_SScaled: return 4;
        case ImageFormat::R16G16_UInt: return 4;
        case ImageFormat::R16G16_SInt: return 4;
        case ImageFormat::R16G16_SFloat: return 4;
        case ImageFormat::R32G32_UInt: return 8;
        case ImageFormat::R32G32_SInt: return 8;
        case ImageFormat::R32G32_SFloat: return 8;
        case ImageFormat::R32G32B32_UInt: return 12;
        case ImageFormat::R32G32B32_SInt: return 12;
        case ImageFormat::R32G32B32_SFloat: return 12;
        case ImageFormat::R32G32B32A32_UInt: return 16;
        case ImageFormat::R32G32B32A32_SInt: return 16;
        case ImageFormat::D16_UNorm: return 2;
        case ImageFormat::D16_UNorm_S8_UInt: return 3;
        case ImageFormat::D24_UNorm_S8_UInt: return 4;
        default: assert(0 && "Not implemented");
    }

    return 64;
};
} // namespace Engine::Gfx::Utils
