#pragma once

namespace Engine
{
enum class ImageFormat
{
    R16G16B16A16_SFloat,
    D16_UNorm
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

enum class BlendFactor
{
    Zero,
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

enum class ColorComponentBits : int
{
    Component_R_Bit = 0x00000001,
    Component_G_Bit = 0x00000002,
    Component_B_Bit = 0x00000004,
    Component_A_Bit = 0x00000008,
};
} // namespace Engine
