#pragma once

#include "Libs/EnumFlags.hpp"
#include <cassert>
#include <cinttypes>
#include <string_view>
namespace Engine::Gfx
{
ENUM_FLAGS(BufferUsage, uint64_t){
    None = 0,
    Transfer_Src = 0x00000001,
    Transfer_Dst = 0x00000002,
    Uniform_Texel = 0x00000004,
    Storage_Texel = 0x00000008,
    Uniform = 0x00000010,
    Storage = 0x00000020,
    Index = 0x00000040,
    Vertex = 0x00000080,
    Indirect = 0x00000100,
};

ENUM_FLAGS(ImageAspect, uint64_t){
    None = 0,
    Color = 0x00000001,
    Depth = 0x00000002,
    Stencil = 0x00000004,
    Metadata = 0x00000008,
    Memory_Plane_0 = 0x00000080,
    Memory_Plane_1 = 0x00000100,
    Memory_Plane_2 = 0x00000200,
};

enum class ShaderResourceFrequency
{
    Global,
    Shader,
    Material,
    Object
};

enum class ImageLayout
{
    Undefined = 0,
    General = 1,
    Color_Attachment = 2,
    Depth_Stencil_Attachment = 3,
    Depth_Stencil_Read_Only = 4,
    Shader_Read_Only = 5,
    Transfer_Src = 6,
    Transfer_Dst = 7,
    Preinitialized = 8,

    Depth_Read_Only_Stencil_Attachment = 1000117000,
    Depth_Attachment_Stencil_Read_Only = 1000117001,
    Depth_Attachment = 1000241000,
    Depth_Read_Only = 1000241001,
    Stencil_Attachment = 1000241002,
    Stencil_Read_Only = 1000241003,
    Read_Only = 1000314000,
    Attachment = 1000314001,
    Present_Src_Khr = 1000001002,
};

enum class ImageViewType
{
    Image_1D,
    Image_2D,
    Image_3D,
    Cubemap,
    Image_1D_Array,
    Image_2D_Array,
    Cube_Array,
};

enum class ImageFormat
{
    R16G16B16A16_SFloat,
    R32G32B32A32_SFloat,
    R16G16B16A16_UNorm,
    R8G8B8A8_UNorm,
    B8G8R8A8_UNorm,
    B8G8R8A8_SRGB,
    R8G8B8A8_SRGB,
    R8G8B8_SRGB,
    R8G8_SRGB,
    R8_SRGB,
    D16_UNorm,
    D16_UNorm_S8_UInt,
    D32_SFLOAT_S8_UInt,
    D24_UNorm_S8_UInt,
    BC7_UNorm_Block,
    BC7_SRGB_UNorm_Block,
    BC3_Unorm_Block,
    BC3_SRGB_Block,
    Invalid
};
ImageFormat MapStringToImageFormat(std::string_view name);

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

ENUM_FLAGS(AccessMask, uint64_t){
    None = 0,
    Indirect_Command_Read = 0x00000001,
    Index_Read = 0x00000002,
    Vertex_Attribute_Read = 0x00000004,
    Uniform_Read = 0x00000008,
    Input_Attachment_Read = 0x00000010,
    Shader_Read = 0x00000020,
    Shader_Write = 0x00000040,
    Color_Attachment_Read = 0x00000080,
    Color_Attachment_Write = 0x00000100,
    Depth_Stencil_Attachment_Read = 0x00000200,
    Depth_Stencil_Attachment_Write = 0x00000400,
    Transfer_Read = 0x00000800,
    Transfer_Write = 0x00001000,
    Host_Read = 0x00002000,
    Host_Write = 0x00004000,
    Memory_Read = 0x00008000,
    Memory_Write = 0x00010000,
};
bool HasWriteAccessMask(AccessMaskFlags flags);
bool HasReadAccessMask(AccessMaskFlags flags);

ENUM_FLAGS(PipelineStage, uint64_t){
    None = 0,
    Top_Of_Pipe = 0x00000001,
    Draw_Indirect = 0x00000002,
    Vertex_Input = 0x00000004,
    Vertex_Shader = 0x00000008,
    Tessellation_Control_Shader = 0x00000010,
    Tessellation_Evaluation_Shader = 0x00000020,
    Geometry_Shader = 0x00000040,
    Fragment_Shader = 0x00000080,
    Early_Fragment_Tests = 0x00000100,
    Late_Fragment_Tests = 0x00000200,
    Color_Attachment_Output = 0x00000400,
    Compute_Shader = 0x00000800,
    Transfer = 0x00001000,
    Bottom_Of_Pipe = 0x00002000,
    Host = 0x00004000,
    All_Graphics = 0x00008000,
    All_Commands = 0x00010000,
};

namespace Utils
{
uint32_t MapImageFormatToByteSize(ImageFormat format);
} // namespace Utils
} // namespace Engine::Gfx
