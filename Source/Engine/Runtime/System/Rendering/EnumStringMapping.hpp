#pragma once
#include "Engine/Driver/GfxDriver/ShaderConfig.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Driver/GfxDriver/ShaderPipelineInfo.hpp"
#include "Engine/Driver/GfxDriver/VertexAttributes.hpp"

#include <string>
namespace Gfx
{
// Enum to string mappings
const char* DescriptorTypeToString(DescriptorType type);
DescriptorType StringToDescriptorType(const std::string& str);

const char* TextureTypeToString(TextureType type);
TextureType StringToTextureType(const std::string& str);

const char* ShaderStageToString(ShaderStageFlags stages);
ShaderStageFlags StringToShaderStage(const std::string& str);

const char* SamplerAddressModeToString(SamplerAddressMode mode);
SamplerAddressMode StringToSamplerAddressMode(const std::string& str);

const char* SamplerMipmapModeToString(SamplerMipmapMode mode);
SamplerMipmapMode StringToSamplerMipmapMode(const std::string& str);

const char* FilterModeToString(FilterMode mode);
FilterMode StringToFilterMode(const std::string& str);

const char* DescriptorSetSemanticsToString(DescriptorSetSemantics semantics);
DescriptorSetSemantics StringToDescriptorSetSemantics(const std::string& str);

const char* MemberDataTypeToString(ShaderPipelineInfo::MemberDataType type);
ShaderPipelineInfo::MemberDataType StringToMemberDataType(const std::string& str);

const char* VertexAttributeSemanticsToString(VertexAttributeSemantics semantics);
VertexAttributeSemantics StringToVertexAttributeSemantics(const std::string& str);

const char* ShaderDynamicStateToString(ShaderDynamicStateFlags state);
// StringToShaderDynamicState already exists in ShaderPipelineInfo.hpp
}

namespace Utils
{
Gfx::PolygonMode MapPolygonMode(const std::string& str);
Gfx::Topology MapTopology(const std::string& str);
Gfx::BlendFactor MapBlendFactor(const std::string& str);
Gfx::CullMode MapCullMode(const std::string str);
Gfx::BlendOp MapBlendOp(const std::string& str);
Gfx::CompareOp MapCompareOp(const std::string& str);
Gfx::StencilOp MapStencilOp(const std::string& str);
Gfx::ColorComponentBits MapColorMask(const std::string& str);

const char* MapPolygonMode(Gfx::PolygonMode mode);
const char* MapTopology(Gfx::Topology topology);
const char* MapStrBlendFactor(Gfx::BlendFactor factor);
const char* MapStrCullMode(Gfx::CullMode cull);
const char* MapStrBlendOp(Gfx::BlendOp op);
const char* MapStrCompareOp(Gfx::CompareOp op);
const char* MapStrStencilOp(Gfx::StencilOp op);
} // namespace Utils
