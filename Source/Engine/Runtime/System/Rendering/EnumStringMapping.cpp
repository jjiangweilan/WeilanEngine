#include "EnumStringMapping.hpp"

namespace Gfx
{

const char* DescriptorTypeToString(DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::Sampler: return "Sampler";
        case DescriptorType::CombinedImageSampler: return "CombinedImageSampler";
        case DescriptorType::SampledImage: return "SampledImage";
        case DescriptorType::StorageImage: return "StorageImage";
        case DescriptorType::UniformTexelBuffer: return "UniformTexelBuffer";
        case DescriptorType::StorageTexelBuffer: return "StorageTexelBuffer";
        case DescriptorType::UniformBuffer: return "UniformBuffer";
        case DescriptorType::StorageBuffer: return "StorageBuffer";
        case DescriptorType::UniformBufferDynamic: return "UniformBufferDynamic";
        case DescriptorType::StorageBufferDynamic: return "StorageBufferDynamic";
        case DescriptorType::InputAttachment: return "InputAttachment";
        case DescriptorType::Invalid: return "Invalid";
    }
    return "Invalid";
}

DescriptorType StringToDescriptorType(const std::string& str)
{
    if (str == "Sampler") return DescriptorType::Sampler;
    if (str == "CombinedImageSampler") return DescriptorType::CombinedImageSampler;
    if (str == "SampledImage") return DescriptorType::SampledImage;
    if (str == "StorageImage") return DescriptorType::StorageImage;
    if (str == "UniformTexelBuffer") return DescriptorType::UniformTexelBuffer;
    if (str == "StorageTexelBuffer") return DescriptorType::StorageTexelBuffer;
    if (str == "UniformBuffer") return DescriptorType::UniformBuffer;
    if (str == "StorageBuffer") return DescriptorType::StorageBuffer;
    if (str == "UniformBufferDynamic") return DescriptorType::UniformBufferDynamic;
    if (str == "StorageBufferDynamic") return DescriptorType::StorageBufferDynamic;
    if (str == "InputAttachment") return DescriptorType::InputAttachment;
    return DescriptorType::Invalid;
}

const char* TextureTypeToString(TextureType type)
{
    switch (type)
    {
        case TextureType::Tex2D: return "Tex2D";
        case TextureType::Tex3D: return "Tex3D";
        case TextureType::TexCube: return "TexCube";
        case TextureType::Invalid: return "Invalid";
    }
    return "Invalid";
}

TextureType StringToTextureType(const std::string& str)
{
    if (str == "Tex2D") return TextureType::Tex2D;
    if (str == "Tex3D") return TextureType::Tex3D;
    if (str == "TexCube") return TextureType::TexCube;
    return TextureType::Invalid;
}

const char* ShaderStageToString(ShaderStageFlags stages)
{
    // Handle common combinations
    if (stages == ShaderStage::None) return "None";
    if (stages == ShaderStage::Vertex) return "Vertex";
    if (stages == ShaderStage::Fragment) return "Fragment";
    if (stages == ShaderStage::Compute) return "Compute";
    if (stages == (ShaderStage::Vertex | ShaderStage::Fragment)) return "VertexFragment";
    if (stages == (ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute)) return "All";
    return "Unknown";
}

ShaderStageFlags StringToShaderStage(const std::string& str)
{
    if (str == "None") return ShaderStage::None;
    if (str == "Vertex") return ShaderStage::Vertex;
    if (str == "Fragment") return ShaderStage::Fragment;
    if (str == "Compute") return ShaderStage::Compute;
    if (str == "VertexFragment") return ShaderStage::Vertex | ShaderStage::Fragment;
    if (str == "All") return ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute;
    return ShaderStage::None;
}

const char* SamplerAddressModeToString(SamplerAddressMode mode)
{
    switch (mode)
    {
        case SamplerAddressMode::Repeat: return "Repeat";
        case SamplerAddressMode::MirroredRepeat: return "MirroredRepeat";
        case SamplerAddressMode::ClampToEdge: return "ClampToEdge";
        case SamplerAddressMode::ClampToBorder: return "ClampToBorder";
        case SamplerAddressMode::MirrorClampToEdge: return "MirrorClampToEdge";
    }
    return "Repeat";
}

SamplerAddressMode StringToSamplerAddressMode(const std::string& str)
{
    if (str == "Repeat") return SamplerAddressMode::Repeat;
    if (str == "MirroredRepeat") return SamplerAddressMode::MirroredRepeat;
    if (str == "ClampToEdge") return SamplerAddressMode::ClampToEdge;
    if (str == "ClampToBorder") return SamplerAddressMode::ClampToBorder;
    if (str == "MirrorClampToEdge") return SamplerAddressMode::MirrorClampToEdge;
    return SamplerAddressMode::Repeat;
}

const char* SamplerMipmapModeToString(SamplerMipmapMode mode)
{
    switch (mode)
    {
        case SamplerMipmapMode::Nearest: return "Nearest";
        case SamplerMipmapMode::Linear: return "Linear";
    }
    return "Nearest";
}

SamplerMipmapMode StringToSamplerMipmapMode(const std::string& str)
{
    if (str == "Linear") return SamplerMipmapMode::Linear;
    return SamplerMipmapMode::Nearest;
}

const char* FilterModeToString(FilterMode mode)
{
    switch (mode)
    {
        case FilterMode::Nearest: return "Nearest";
        case FilterMode::Linear: return "Linear";
    }
    return "Nearest";
}

FilterMode StringToFilterMode(const std::string& str)
{
    if (str == "Linear") return FilterMode::Linear;
    return FilterMode::Nearest;
}

const char* DescriptorSetSemanticsToString(DescriptorSetSemantics semantics)
{
    switch (semantics)
    {
        case DescriptorSetSemantics::Global: return "Global";
        case DescriptorSetSemantics::Material: return "Material";
        case DescriptorSetSemantics::Object: return "Object";
    }
    return "Global";
}

DescriptorSetSemantics StringToDescriptorSetSemantics(const std::string& str)
{
    if (str == "Material") return DescriptorSetSemantics::Material;
    if (str == "Object") return DescriptorSetSemantics::Object;
    return DescriptorSetSemantics::Global;
}

const char* MemberDataTypeToString(ShaderPipelineInfo::MemberDataType type)
{
    switch (type)
    {
        case ShaderPipelineInfo::MemberDataType::Bool: return "Bool";
        case ShaderPipelineInfo::MemberDataType::Float: return "Float";
        case ShaderPipelineInfo::MemberDataType::UInt: return "UInt";
        case ShaderPipelineInfo::MemberDataType::Int: return "Int";
        case ShaderPipelineInfo::MemberDataType::Structure: return "Structure";
    }
    return "Float";
}

ShaderPipelineInfo::MemberDataType StringToMemberDataType(const std::string& str)
{
    if (str == "Bool") return ShaderPipelineInfo::MemberDataType::Bool;
    if (str == "Float") return ShaderPipelineInfo::MemberDataType::Float;
    if (str == "UInt") return ShaderPipelineInfo::MemberDataType::UInt;
    if (str == "Int") return ShaderPipelineInfo::MemberDataType::Int;
    if (str == "Structure") return ShaderPipelineInfo::MemberDataType::Structure;
    return ShaderPipelineInfo::MemberDataType::Float;
}

const char* VertexAttributeSemanticsToString(VertexAttributeSemantics semantics)
{
    switch (semantics)
    {
        case VertexAttributeSemantics::Position: return "Position";
        case VertexAttributeSemantics::Normal: return "Normal";
        case VertexAttributeSemantics::Tangent: return "Tangent";
        case VertexAttributeSemantics::Texcoord: return "Texcoord";
        case VertexAttributeSemantics::Color: return "Color";
        case VertexAttributeSemantics::Bone: return "Bone";
    }
    return "Position";
}

VertexAttributeSemantics StringToVertexAttributeSemantics(const std::string& str)
{
    if (str == "Position") return VertexAttributeSemantics::Position;
    if (str == "Normal") return VertexAttributeSemantics::Normal;
    if (str == "Tangent") return VertexAttributeSemantics::Tangent;
    if (str == "Texcoord") return VertexAttributeSemantics::Texcoord;
    if (str == "Color") return VertexAttributeSemantics::Color;
    if (str == "Bone") return VertexAttributeSemantics::Bone;
    return VertexAttributeSemantics::Position;
}

const char* ShaderDynamicStateToString(ShaderDynamicStateFlags state)
{
    if (state == ShaderDynamicState::None) return "None";
    if (state == ShaderDynamicState::DepthBiasEnable) return "DepthBiasEnable";
    if (state == ShaderDynamicState::DepthBias) return "DepthBias";
    if (state == (ShaderDynamicState::DepthBiasEnable | ShaderDynamicState::DepthBias)) return "DepthBiasEnable|DepthBias";
    return "None";
}

} // namespace Gfx

namespace Utils
{
Gfx::ColorComponentBits MapColorMask(const std::string& str)
{
    Gfx::ColorComponentBits mask = 0;
    if (str.find("R") != str.npos)
    {
        mask |= Gfx::ColorComponentBit::Component_R_Bit;
    }
    if (str.find("G") != str.npos)
    {
        mask |= Gfx::ColorComponentBit::Component_G_Bit;
    }
    if (str.find("B") != str.npos)
    {
        mask |= Gfx::ColorComponentBit::Component_B_Bit;
    }
    if (str.find("A") != str.npos)
    {
        mask |= Gfx::ColorComponentBit::Component_A_Bit;
    }

    return mask;
}

Gfx::BlendFactor MapBlendFactor(const std::string& str)
{
    if (str == "zero")
        return Gfx::BlendFactor::Zero;
    else if (str == "one")
        return Gfx::BlendFactor::One;
    else if (str == "srcColor")
        return Gfx::BlendFactor::Src_Color;
    else if (str == "oneMinusSrcColor")
        return Gfx::BlendFactor::One_Minus_Src_Color;
    else if (str == "dstColor")
        return Gfx::BlendFactor::Dst_Color;
    else if (str == "oneMinusDstColor")
        return Gfx::BlendFactor::One_Minus_Dst_Color;
    else if (str == "srcAlpha")
        return Gfx::BlendFactor::Src_Alpha;
    else if (str == "oneMinusSrcAlpha")
        return Gfx::BlendFactor::One_Minus_Src_Alpha;
    else if (str == "dstAlpha")
        return Gfx::BlendFactor::Dst_Alpha;
    else if (str == "oneMinusDstAlpha")
        return Gfx::BlendFactor::One_Minus_Dst_Alpha;
    else if (str == "constantColor")
        return Gfx::BlendFactor::Constant_Color;
    else if (str == "oneMinusConstantColor")
        return Gfx::BlendFactor::One_Minus_Constant_Color;
    else if (str == "constantAlpha")
        return Gfx::BlendFactor::Constant_Alpha;
    else if (str == "oneMinusConstantAlpha")
        return Gfx::BlendFactor::One_Minus_Constant_Alpha;
    else if (str == "srcAlphaSaturate")
        return Gfx::BlendFactor::Src_Alpha_Saturate;
    else if (str == "src1Color")
        return Gfx::BlendFactor::Src1_Color;
    else if (str == "oneMinusSrc1Color")
        return Gfx::BlendFactor::One_Minus_Src1_Color;
    else if (str == "src1Alpha")
        return Gfx::BlendFactor::Src1_Alpha;
    else if (str == "oneMinusSrc1Alpha")
        return Gfx::BlendFactor::One_Minus_Src1_Alpha;

    return Gfx::BlendFactor::Src_Alpha;
}

Gfx::CullMode MapCullMode(const std::string str)
{
    if (str == "none" || str == "off")
        return Gfx::CullMode::None;
    else if (str == "front")
        return Gfx::CullMode::Front;
    else if (str == "back")
        return Gfx::CullMode::Back;
    else if (str == "both")
        return Gfx::CullMode::Both;

    return Gfx::CullMode::Back;
}

Gfx::BlendOp MapBlendOp(const std::string& str)
{
    if (str == "add")
        return Gfx::BlendOp::Add;
    else if (str == "subtract")
        return Gfx::BlendOp::Subtract;
    else if (str == "reverseSubtract")
        return Gfx::BlendOp::Reverse_Subtract;
    else if (str == "min")
        return Gfx::BlendOp::Min;
    else if (str == "max")
        return Gfx::BlendOp::Max;

    return Gfx::BlendOp::Add;
}

const char* MapStrBlendFactor(Gfx::BlendFactor factor)
{
    switch (factor)
    {
        case (Gfx::BlendFactor::Zero): return "zero";
        case (Gfx::BlendFactor::One): return "one";
        case (Gfx::BlendFactor::Src_Color): return "srcColor";
        case (Gfx::BlendFactor::One_Minus_Src_Color): return "oneMinusSrcColor";
        case (Gfx::BlendFactor::Dst_Color): return "dstColor";
        case (Gfx::BlendFactor::One_Minus_Dst_Color): return "oneMinusDstColor";
        case (Gfx::BlendFactor::Src_Alpha): return "srcAlpha";
        case (Gfx::BlendFactor::One_Minus_Src_Alpha): return "oneMinusSrcAlpha";
        case (Gfx::BlendFactor::Dst_Alpha): return "dstAlpha";
        case (Gfx::BlendFactor::One_Minus_Dst_Alpha): return "oneMinusDstAlpha";
        case (Gfx::BlendFactor::Constant_Color): return "constantColor";
        case (Gfx::BlendFactor::One_Minus_Constant_Color): return "oneMinusConstantColor";
        case (Gfx::BlendFactor::Constant_Alpha): return "constantAlpha";
        case (Gfx::BlendFactor::One_Minus_Constant_Alpha): return "oneMinusConstantAlpha";
        case (Gfx::BlendFactor::Src_Alpha_Saturate): return "srcAlphaSaturate";
        case (Gfx::BlendFactor::Src1_Color): return "src1Color";
        case (Gfx::BlendFactor::One_Minus_Src1_Color): return "oneMinusSrc1Color";
        case (Gfx::BlendFactor::Src1_Alpha): return "src1Alpha";
        case (Gfx::BlendFactor::One_Minus_Src1_Alpha): return "oneMinusSrc1Alpha";
    }

    return "zero";
}

const char* MapStrCullMode(Gfx::CullMode cull)
{
    switch (cull)
    {
        case (Gfx::CullMode::None): return "none";
        case (Gfx::CullMode::Front): return "front";
        case (Gfx::CullMode::Back): return "back";
        case (Gfx::CullMode::Both): return "both";
    }

    return "back";
}

const char* MapStrBlendOp(Gfx::BlendOp op)
{
    switch (op)
    {
        case (Gfx::BlendOp::Add): return "add";
        case (Gfx::BlendOp::Subtract): return "subtract";
        case (Gfx::BlendOp::Reverse_Subtract): return "reverseSubtract";
        case (Gfx::BlendOp::Min): return "min";
        case (Gfx::BlendOp::Max): return "max";
    }

    return "add";
}

const char* MapStrCompareOp(Gfx::CompareOp op)
{
    switch (op)
    {
        case (Gfx::CompareOp::Never): return "never";
        case (Gfx::CompareOp::Equal): return "equal";
        case (Gfx::CompareOp::Less_or_Equal): return "lessOrEqual";
        case (Gfx::CompareOp::Greater): return "greater";
        case (Gfx::CompareOp::Not_Equal): return "notEqual";
        case (Gfx::CompareOp::Greater_Or_Equal): return "greaterOrEqual";
        case (Gfx::CompareOp::Always): return "always";
        case (Gfx::CompareOp::Less): return "always";
    }

    return "never";
}

Gfx::CompareOp MapCompareOp(const std::string& str)
{
    if ("never" == str)
        return Gfx::CompareOp::Never;
    else if ("less" == str)
        return Gfx::CompareOp::Less;
    else if ("equal" == str)
        return Gfx::CompareOp::Equal;
    else if ("lessOrEqual" == str)
        return Gfx::CompareOp::Less_or_Equal;
    else if ("greater" == str)
        return Gfx::CompareOp::Greater;
    else if ("notEqual" == str)
        return Gfx::CompareOp::Not_Equal;
    else if ("greaterOrEqual" == str)
        return Gfx::CompareOp::Greater_Or_Equal;
    else if ("always" == str)
        return Gfx::CompareOp::Always;

    return Gfx::CompareOp::Always;
}

Gfx::StencilOp MapStencilOp(const std::string& str)
{
    if ("keep" == str)
        return Gfx::StencilOp::Keep;
    else if ("zero" == str)
        return Gfx::StencilOp::Zero;
    else if ("replace" == str)
        return Gfx::StencilOp::Replace;
    else if ("incrementAndClamp" == str)
        return Gfx::StencilOp::Increment_And_Clamp;
    else if ("decrementAndClamp" == str)
        return Gfx::StencilOp::Decrement_And_Clamp;
    else if ("invert" == str)
        return Gfx::StencilOp::Invert;
    else if ("incrementAndWrap" == str)
        return Gfx::StencilOp::Increment_And_Wrap;
    else if ("decrementAndWrap == str")
        return Gfx::StencilOp::Decrement_And_Wrap;

    return Gfx::StencilOp::Keep;
}

const char* MapStrStencilOp(Gfx::StencilOp op)
{
    switch (op)
    {
        case (Gfx::StencilOp::Keep): return "keep";
        case (Gfx::StencilOp::Zero): return "zero";
        case (Gfx::StencilOp::Replace): return "replace";
        case (Gfx::StencilOp::Increment_And_Clamp): return "increment_And_Clamp";
        case (Gfx::StencilOp::Decrement_And_Clamp): return "decrement_And_Clamp";
        case (Gfx::StencilOp::Invert): return "invert";
        case (Gfx::StencilOp::Increment_And_Wrap): return "increment_And_Wrap";
        case (Gfx::StencilOp::Decrement_And_Wrap): return "decrement_And_Wrap";
    }

    return "keep";
}

Gfx::PolygonMode MapPolygonMode(const std::string& str)
{
    if (str == "line")
        return Gfx::PolygonMode::Line;
    if (str == "point")
        return Gfx::PolygonMode::Point;
    if (str == "fill")
        return Gfx::PolygonMode::Fill;

    return Gfx::PolygonMode::Fill;
}

const char* MapPolygonMode(Gfx::PolygonMode mode)
{
    switch (mode)
    {
        case Gfx::PolygonMode::Point: return "point";
        case Gfx::PolygonMode::Line: return "line";
        case Gfx::PolygonMode::Fill: return "fill";
    }

    return "fill";
}

Gfx::Topology MapTopology(const std::string& str)
{
    if (str == "triangleStrip")
        return Gfx::Topology::TriangleStrip;
    else if (str == "lineStrip")
    {
        return Gfx::Topology::LineStrip;
    }
    else if (str == "triangleList")
    {
        return Gfx::Topology::TriangleList;
    }
    else if (str == "lineList")
        return Gfx::Topology::LineList;

    return Gfx::Topology::TriangleList;
}

const char* MapTopology(Gfx::Topology topology)
{
    switch (topology)
    {
        case (Gfx::Topology::TriangleList): return "triangleList";
        case (Gfx::Topology::TriangleStrip): return "triangleStrip";
        case (Gfx::Topology::LineStrip): return "lineStrip";
        case (Gfx::Topology::LineList): return "lineList";
    }

    return "triangleList";
}
} // namespace Utils
