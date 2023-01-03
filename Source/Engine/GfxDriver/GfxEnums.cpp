#include "GfxEnums.hpp"

namespace Engine::Gfx::Utils
{
uint32_t MapImageFormatToBitSize(ImageFormat format)
{
    switch (format)
    {
        case ImageFormat::R16G16B16A16_SFloat: return 64;
        case ImageFormat::R8G8B8A8_UNorm:
        case ImageFormat::B8G8R8A8_UNorm:
        case ImageFormat::B8G8R8A8_SRGB: return 32;
        case ImageFormat::R8_SRGB: return 8;
        case ImageFormat::R8G8_SRGB: return 16;
        case ImageFormat::R8G8B8_SRGB: return 24;
        case ImageFormat::R8G8B8A8_SRGB: return 32;
        case ImageFormat::D16_UNorm: return 16;
        case ImageFormat::D16_UNorm_S8_UInt: return 24;
        case ImageFormat::D24_UNorm_S8_UInt: return 32;
        default: assert(0 && "Not implemented");
    }

    return 64;
};

uint32_t MapImageFormatToByteSize(ImageFormat format)
{
    switch (format)
    {
        case ImageFormat::R16G16B16A16_SFloat: return 8;
        case ImageFormat::R8G8B8A8_UNorm:
        case ImageFormat::B8G8R8A8_UNorm:
        case ImageFormat::B8G8R8A8_SRGB: return 4;
        case ImageFormat::R8G8B8A8_SRGB: return 4;
        case ImageFormat::R8G8B8_SRGB: return 3;
        case ImageFormat::R8G8_SRGB: return 2;
        case ImageFormat::R8_SRGB: return 1;
        case ImageFormat::D16_UNorm: return 2;
        case ImageFormat::D16_UNorm_S8_UInt: return 3;
        case ImageFormat::D24_UNorm_S8_UInt: return 4;
        default: assert(0 && "Not implemented");
    }

    return 64;
};

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
} // namespace Engine::Gfx::Utils
