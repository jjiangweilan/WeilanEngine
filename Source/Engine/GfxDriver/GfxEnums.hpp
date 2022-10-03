#pragma once

#include <cinttypes>
#include <cassert>
namespace Engine::Gfx
{
    enum class BufferUsage : uint32_t
    {
        Vertex = 0x0001,
        Index = 0x0002,
        Uniform = 0x0004,
        Unknown = 0xffff
    };

    enum class ShaderResourceFrequency
    {
        Global,
        Shader,
        Material,
        Object
    };

    enum class ImageFormat
    {
        R16G16B16A16_SFloat,
        R8G8B8A8_UNorm,
        B8G8R8A8_UNorm,
        B8G8R8A8_SRgb,
        D16_UNorm,
        D16_UNorm_S8_UInt,
        D24_UNorm_S8_UInt
    };

    enum class MultiSampling
    {
        Sample_Count_1,
        Sample_Count_2,
        Sample_Count_4,
        Sample_Count_8,
        Sample_Count_16,
        Sample_Count_32,
        Sample_Count_64
    };

    enum class AttachmentLoadOperation
    {
        Load,
        Clear,
        DontCare,
    };

    enum class AttachmentStoreOperation
    {
        Store,
        DontCare,
    };

    enum class CullMode
    {
        None,
        Front,
        Back,
        Both
    };

    namespace ImageUsage
    {
        enum Enum : uint32_t
        {
            ColorAttachment = 0x1,
            DepthStencilAttachment = 0x2,
            Texture = 0x4,
            TransferSrc = 0x8,
            TransferDst = 0x10
        };
    }
    typedef uint32_t ImageUsageFlags;

    enum class CompareOp
    {
        Never,
        Less,
        Equal,
        Less_or_Equal,
        Greater,
        Not_Equal,
        Greater_Or_Equal,
        Always
    };

    enum class StencilOp
    {
        Keep,
        Zero,
        Replace,
        Increment_And_Clamp,
        Decrement_And_Clamp,
        Invert,
        Increment_And_Wrap,
        Decrement_And_Wrap
    };

    enum class BlendFactor : int
    {
        Zero = 0,
        One,
        Src_Color,
        One_Minus_Src_Color,
        Dst_Color,
        One_Minus_Dst_Color,
        Src_Alpha,
        One_Minus_Src_Alpha,
        Dst_Alpha,
        One_Minus_Dst_Alpha,
        Constant_Color,
        One_Minus_Constant_Color,
        Constant_Alpha,
        One_Minus_Constant_Alpha,
        Src_Alpha_Saturate,
        Src1_Color,
        One_Minus_Src1_Color,
        Src1_Alpha,
        One_Minus_Src1_Alpha,
    };

    enum class BlendOp
    {
        Add,
        Subtract,
        Reverse_Subtract,
        Min,
        Max,
    };

    namespace ColorComponentBit
    {
        enum Enum
        {
            Component_R_Bit = 0x00000001,
            Component_G_Bit = 0x00000002,
            Component_B_Bit = 0x00000004,
            Component_A_Bit = 0x00000008,
            Component_All_Bits = 0x0000000f,
        };
    }
    typedef uint32_t ColorComponentBits;

namespace Utils
{
    uint32_t MapImageFormatToBitSize(ImageFormat format);
    uint32_t MapImageFormatToByteSize(ImageFormat format);
}
}
