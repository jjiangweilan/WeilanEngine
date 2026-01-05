#include "ShaderPipelineInfoSerialization.hpp"
#include "Engine/Runtime/System/Rendering/EnumStringMapping.hpp"

namespace Gfx
{

void to_json(nlohmann::json& j, const ShaderPipelineInfo::MemberDataType& type)
{
    j = MemberDataTypeToString(type);
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::MemberDataType& type)
{
    type = StringToMemberDataType(j.get<std::string>());
}

void to_json(nlohmann::json& j, const DescriptorSetSemantics& semantics)
{
    j = DescriptorSetSemanticsToString(semantics);
}

void from_json(const nlohmann::json& j, DescriptorSetSemantics& semantics)
{
    semantics = StringToDescriptorSetSemantics(j.get<std::string>());
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::BufferMember& member)
{
    j = nlohmann::json{
        {"name", member.name},
        {"type", member.type},
        {"columnCount", member.columnCount},
        {"rowCount", member.rowCount},
        {"offset", member.offset},
        {"count", member.count},
        {"byteSize", member.byteSize},
        {"attributes", member.attributes}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::BufferMember& member)
{
    member.name = j.value("name", "");
    member.type = j.value("type", ShaderPipelineInfo::MemberDataType::Float);
    member.columnCount = j.value("columnCount", 1u);
    member.rowCount = j.value("rowCount", 1u);
    member.offset = j.value("offset", 0u);
    member.count = j.value("count", 0u);
    member.byteSize = j.value("byteSize", 0u);
    if (j.contains("attributes"))
        member.attributes = j["attributes"].get<std::vector<std::string>>();
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::Binding& binding)
{
    j = nlohmann::json{
        {"name", binding.name},
        {"bindingNum", binding.bindingNum},
        {"descriptorCount", binding.descriptorCount},
        {"stages", binding.stages},
        {"descriptorType", DescriptorTypeToString(binding.descriptorType)},
        {"textureType", TextureTypeToString(binding.textureType)},
        {"isTextureArray", binding.isTextureArray},
        {"bufferMembers", binding.bufferMembers},
        {"byteSize", binding.byteSize},
        {"samplerIndex", binding.samplerIndex}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::Binding& binding)
{
    binding.name = j.value("name", "");
    binding.shaderBindingHandle = ShaderBindingHandle(binding.name);
    binding.bindingNum = j.value("bindingNum", 0u);
    binding.descriptorCount = j.value("descriptorCount", 0u);
    binding.stages = static_cast<ShaderStageFlags>(j.value("stages", 0));
    binding.descriptorType = StringToDescriptorType(j.value("descriptorType", std::string("Invalid")));
    binding.textureType = StringToTextureType(j.value("textureType", std::string("Invalid")));
    binding.isTextureArray = j.value("isTextureArray", false);
    if (j.contains("bufferMembers"))
        binding.bufferMembers = j["bufferMembers"].get<std::vector<ShaderPipelineInfo::BufferMember>>();
    binding.byteSize = j.value("byteSize", 0u);
    binding.samplerIndex = j.value("samplerIndex", -1);
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::VertexAttribute& attr)
{
    j = nlohmann::json{
        {"name", attr.name},
        {"semanticName", VertexAttributeSemanticsToString(attr.semanticName)},
        {"semanticIndex", attr.semanticIndex},
        {"location", attr.location},
        {"size", attr.size},
        {"format", MapGfxFormatToString(attr.format)}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::VertexAttribute& attr)
{
    attr.name = j.value("name", "");
    attr.semanticName = StringToVertexAttributeSemantics(j.value("semanticName", std::string("Position")));
    attr.semanticIndex = j.value("semanticIndex", 0);
    attr.location = j.value("location", 0);
    attr.size = j.value("size", 0);
    attr.format = MapStringToGfxFormat(j.value("format", std::string("Invalid")));
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::FragmentOutput& output)
{
    j = nlohmann::json{
        {"format", MapGfxFormatToString(output.format)},
        {"location", output.location}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::FragmentOutput& output)
{
    output.format = MapStringToGfxFormat(j.value("format", std::string("Invalid")));
    output.location = j.value("location", 0);
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::SamplerConfig& config)
{
    j = nlohmann::json{
        {"addressModeU", SamplerAddressModeToString(config.addressModeU)},
        {"addressModeV", SamplerAddressModeToString(config.addressModeV)},
        {"addressModeW", SamplerAddressModeToString(config.addressModeW)},
        {"mipmapMode", SamplerMipmapModeToString(config.mipmapMode)},
        {"minFilter", FilterModeToString(config.minFilter)},
        {"magFilter", FilterModeToString(config.magFilter)},
        {"anisotropic", config.anisotropic},
        {"enableCompare", config.enableCompare}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::SamplerConfig& config)
{
    config.addressModeU = StringToSamplerAddressMode(j.value("addressModeU", std::string("Repeat")));
    config.addressModeV = StringToSamplerAddressMode(j.value("addressModeV", std::string("Repeat")));
    config.addressModeW = StringToSamplerAddressMode(j.value("addressModeW", std::string("Repeat")));
    config.mipmapMode = StringToSamplerMipmapMode(j.value("mipmapMode", std::string("Nearest")));
    config.minFilter = StringToFilterMode(j.value("minFilter", std::string("Nearest")));
    config.magFilter = StringToFilterMode(j.value("magFilter", std::string("Nearest")));
    config.anisotropic = j.value("anisotropic", false);
    config.enableCompare = j.value("enableCompare", false);
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::DescriptorSet& set)
{
    j = nlohmann::json{
        {"name", set.name},
        {"setNum", set.setNum},
        {"semantics", set.semantics},
        {"bindings", set.bindings},
        {"samplerConfigs", set.samplerConfigs}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::DescriptorSet& set)
{
    set.name = j.value("name", "");
    set.setNum = j.value("setNum", 0);
    set.semantics = StringToDescriptorSetSemantics(j.value("semantics", std::string("Global")));
    if (j.contains("bindings"))
        set.bindings = j["bindings"].get<std::vector<ShaderPipelineInfo::Binding>>();
    if (j.contains("samplerConfigs"))
        set.samplerConfigs = j["samplerConfigs"].get<std::vector<ShaderPipelineInfo::SamplerConfig>>();
    set.UpdateBindingIndex();
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo::PushConstant& pc)
{
    j = nlohmann::json{
        {"size", pc.size},
        {"stages", ShaderStageToString(pc.stages)}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo::PushConstant& pc)
{
    pc.size = j.value("size", 0u);
    pc.stages = static_cast<Gfx::ShaderStageFlags>(j.value("stages", 0));
}

void to_json(nlohmann::json& j, const ShaderPipelineInfo& info)
{
    j = nlohmann::json{
        {"name", info.name},
        {"vertexShaderName", info.vertexShaderName},
        {"fragmentShaderName", info.fragmentShaderName},
        {"computeShaderName", info.computeShaderName},
        {"isVertexInterleaved", info.isVertexInterleaved},
        {"vertexInputs", info.vertexInputs},
        {"fragmentOutputs", info.fragmentOutputs},
        {"descriptorSets", info.descriptorSets},
        {"pushConstants", info.pushConstants},
        {"shaderDynamicStateFlags", info.shaderDynamicStateFlags}
    };
}

void from_json(const nlohmann::json& j, ShaderPipelineInfo& info)
{
    info.name = j.value("name", "");
    info.vertexShaderName = j.value("vertexShaderName", "");
    info.fragmentShaderName = j.value("fragmentShaderName", "");
    info.computeShaderName = j.value("computeShaderName", "");
    info.isVertexInterleaved = j.value("isVertexInterleaved", false);
    if (j.contains("vertexInputs"))
        info.vertexInputs = j["vertexInputs"].get<std::vector<ShaderPipelineInfo::VertexAttribute>>();
    if (j.contains("fragmentOutputs"))
        info.fragmentOutputs = j["fragmentOutputs"].get<std::vector<ShaderPipelineInfo::FragmentOutput>>();
    if (j.contains("descriptorSets"))
        info.descriptorSets = j["descriptorSets"].get<std::vector<ShaderPipelineInfo::DescriptorSet>>();
    if (j.contains("pushConstants"))
        info.pushConstants = j["pushConstants"].get<std::vector<ShaderPipelineInfo::PushConstant>>();
    info.shaderDynamicStateFlags = static_cast<ShaderDynamicStateFlags>(j.value("shaderDynamicStateFlags", 0));
}

} // namespace Gfx
