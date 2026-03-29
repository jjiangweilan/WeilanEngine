#pragma once

#include "Engine/Library/EnumFlags.hpp"
#include <cassert>
#include <cinttypes>
#include <string_view>
namespace Gfx
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
    AccelerationStructure = 0x00000200,
    AccelerationStructureBuildInput = 0x00000400,
    ShaderDeviceAddress = 0x00000800,
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
    Pass,
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

    Dynamic, // this indicates command buffer should dynamically detect image's layout when transfering image layout as
             // source
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

enum class GfxFormat
{
    R16G16B16A16_SFloat = 0,
    R16G16B16A16_UNorm,
    R8G8B8A8_UNorm,
    B8G8R8A8_UNorm,
    B8G8R8A8_SRGB,
    R8G8B8A8_SRGB,
    R8G8B8_SRGB,
    R8G8_SRGB,
    R8_SRGB,
    R16G16_UNorm,
    R16G16_SNorm,
    R16G16_UScaled,
    R16G16_SScaled,
    R16G16_UInt,
    R16G16_SInt,
    R16G16_SFloat,
    R32G32_UInt,
    R32G32_SInt,
    R32G32_SFloat,
    R32G32B32_UInt,
    R32G32B32_SInt,
    R32G32B32_SFloat,
    R32G32B32A32_UInt,
    R32G32B32A32_SInt,
    R32G32B32A32_SFloat,
    R16G16B16_UNorm,
    R16G16B16_SNorm,
    R16G16B16_UScaled,
    R16G16B16_SScaled,
    R16G16B16_UInt,
    R16G16B16_SInt,
    R16G16B16_SFloat,
    R32_SFloat,
    R16_SFloat,
    R16_UNorm,
    D16_UNorm,
    D16_UNorm_S8_UInt,
    D32_SFloat,
    D32_SFLOAT_S8_UInt,
    D24_UNorm_S8_UInt,
    BC7_UNorm_Block,
    BC7_SRGB_UNorm_Block,
    BC3_Unorm_Block,
    BC3_SRGB_Block,
    B10G11R11_UFloat_Pack32,
    A2B10G10R10_UNorm,
    R8_UNorm,
    R8G8_UNorm,
    R8_UInt,
    Invalid
};

GfxFormat GetGfxFormat(int channelBits, int channels, bool linear);

GfxFormat MapStringToGfxFormat(std::string_view name);
const char* MapGfxFormatToString(GfxFormat format);

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
    MAX_COUNT
};

enum class AttachmentStoreOperation
{
    Store,
    DontCare,
    MAX_COUNT
};

enum class CullMode
{
    None,
    Front,
    Back,
    Both,
    MAX_COUNT
};

enum class Topology
{
    TriangleList,
    TriangleStrip,
    LineStrip,
    LineList,
    MAX_COUNT
};

enum class PolygonMode
{
    Fill = 0,
    Line = 1,
    Point = 2,
    MAX_COUNT
};

namespace ImageUsage
{
enum Enum : uint32_t
{
    None = 0x0,
    ColorAttachment = 0x1,
    DepthStencilAttachment = 0x2,
    Texture = 0x4,
    Storage = 0x8,

    TransferSrc = 0x10, // internal
    TransferDst = 0x20  // internal
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
bool IsDepthStencilFormat(GfxFormat format);
bool HasStencil(GfxFormat format);
bool IsColoFormat(GfxFormat format);
uint32_t MapGfxFormatToByteSize(GfxFormat format);

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

enum class ShaderStage
{
    None = 0,
    Vertex = 1,
    Fragment = 1 << 2,
    Compute = 1 << 3,
};
ENUM_FLAGS(ShaderStage, int);

enum class DescriptorType
{
    Sampler = 0,
    CombinedImageSampler = 1,
    SampledImage = 2,
    StorageImage = 3,
    UniformTexelBuffer = 4,
    StorageTexelBuffer = 5,
    UniformBuffer = 6,
    StorageBuffer = 7,
    UniformBufferDynamic = 8,
    StorageBufferDynamic = 9,
    InputAttachment = 10,
    AccelerationStructure = 11,
    Invalid = 12
};

enum class SamplerAddressMode
{
    Repeat = 0,
    MirroredRepeat = 1,
    ClampToEdge = 2,
    ClampToBorder = 3,
    MirrorClampToEdge = 4,
};

enum class SamplerMipmapMode
{
    Nearest = 0,
    Linear = 1,
};

enum class FilterMode
{
    Nearest = 0,
    Linear = 1,
};

enum class TextureType
{
    Invalid,
    Tex2D,
    Tex3D,
    TexCube,
};

} // namespace Gfx
