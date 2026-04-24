#pragma once
#include "ShaderPipelineInfo.hpp"
#include <nlohmann/json.hpp>

namespace Gfx
{

// Forward declarations for JSON conversion
void to_json(nlohmann::json& j, const ShaderPipelineInfo::MemberDataType& type);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::MemberDataType& type);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::BufferMember& member);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::BufferMember& member);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::Binding& binding);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::Binding& binding);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::VertexAttribute& attr);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::VertexAttribute& attr);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::FragmentOutput& output);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::FragmentOutput& output);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::SamplerConfig& config);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::SamplerConfig& config);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::DescriptorSet& set);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::DescriptorSet& set);

void to_json(nlohmann::json& j, const ShaderPipelineInfo::PushConstant& pc);
void from_json(const nlohmann::json& j, ShaderPipelineInfo::PushConstant& pc);

void to_json(nlohmann::json& j, const ShaderPipelineInfo& info);
void from_json(const nlohmann::json& j, ShaderPipelineInfo& info);

void to_json(nlohmann::json& j, const DescriptorSetSemantics& semantics);
void from_json(const nlohmann::json& j, DescriptorSetSemantics& semantics);

} // namespace Gfx
