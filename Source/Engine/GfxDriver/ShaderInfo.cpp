#include "ShaderInfo.hpp"
#include "Libs/Utils.hpp"
#include <regex>
#include <spdlog/spdlog.h>

namespace Engine::Gfx::ShaderInfo
{
namespace Utils
{
// we use binding name to configure some vulkan states
// we don't want these configure as part of the name
// e.g. sampler binding have _point _clamp etc... we don't want them in our binding name
std::string RemoveShaderNameConfigText(std::string text)
{
    const char* configPattern[] = {"_sampler", "_point", "_clamp", "_linear", "repeat"};

    for (auto c : configPattern)
    {
        std::regex pattern(c);
        text = std::regex_replace(text, pattern, "");
    }

    return text;
}

Texture::Type MapStringToTextureType(const std::string& typeStr)
{
    if (typeStr == "sampler2D")
        return Texture::Type::Tex2D;
    if (typeStr == "samplerCube")
        return Texture::Type::TexCube;

    assert(false && "non handled Texture::Type");
    return Texture::Type::Tex2D;
}

ShaderDataType MapShaderDataType(const std::string& typeStr)
{
    if (typeStr == "vec4")
        return ShaderDataType::Vec4;
    else if (typeStr == "vec3")
        return ShaderDataType::Vec3;
    else if (typeStr == "vec2")
        return ShaderDataType::Vec2;
    else if (typeStr == "mat4")
        return ShaderDataType::Mat4;
    else if (typeStr == "mat3")
        return ShaderDataType::Mat3;
    else if (typeStr == "mat2")
        return ShaderDataType::Mat2;
    else if (typeStr == "float")
        return ShaderDataType::Float;
    else if (typeStr[0] == '_')
        return ShaderDataType::Structure;

    assert(0 && "Shader Data Type map failed");
    return ShaderDataType::Vec4;
}

uint32_t MapTypeToSize(const std::string& typeName, const std::string& memberName)
{
    std::string lowerName = Engine::Utils::strTolower(memberName);

    uint32_t baseTypeSize = 4;
    if (lowerName.find("_16") != lowerName.npos)
        baseTypeSize = 2;
    else if (lowerName.find("_8") != lowerName.npos)
        baseTypeSize = 1;
    // if (lowerName.find("color") != lowerName.npos)
    //{
    //     if (lowerName.find("16") != lowerName.npos)
    //         baseTypeSize = 2;
    //     else
    //         baseTypeSize = sizeof(char);
    // }
    if (typeName == "vec4")
        return baseTypeSize * 4;
    else if (typeName == "vec3")
        return baseTypeSize * 3;
    else if (typeName == "vec2")
        return baseTypeSize * 2;
    else if (typeName == "mat2")
        return baseTypeSize * 4;
    else if (typeName == "mat3")
        return baseTypeSize * 9;
    else if (typeName == "mat4")
        return baseTypeSize * 16;
    else if (typeName == "float")
        return baseTypeSize;

    SPDLOG_ERROR("Failed to map stride");
    return 0;
}

void Process(StructuredData& data, nlohmann::json& typeJson, nlohmann::json& root, const std::string& memberName)
{
    data.type = MapShaderDataType(typeJson);
    if (data.type == ShaderDataType::Structure)
    {
        nlohmann::json& typeInfoJson = root["types"][typeJson.get<std::string>()];
        data.name = typeInfoJson.at("name");
        auto& membersJson = typeInfoJson.at("members");
        uint32_t offset = 0;
        for (auto& memJson : membersJson)
        {
            Member member;
            member.name = memJson.at("name");
            member.data = MakeUnique<StructuredData>();
            Process(*member.data, memJson.at("type"), root, member.name);

            if (memJson.contains("array"))
            {
                for (auto& dimJson : memJson["array"])
                {
                    member.dimension.push_back(dimJson);
                }
            }
            else
            {
                member.dimension.push_back(1);
            }

            // if reflection reports an offset, then we should use it
            // if not, we use the calmulative offset so far
            if (memJson.contains("offset"))
            {
                member.offset = memJson.at("offset");
            }
            else
            {
                member.offset = offset;
            }

            offset += member.data->size;
            data.members.insert(std::make_pair(member.name, std::move(member)));
        }

        data.size = 0;
        for (auto& iter : data.members)
        {
            int count = iter.second.dimension[0];

            for (int i = 1; i < iter.second.dimension.size(); ++i)
            {
                count *= iter.second.dimension[i];
            }

            data.size += iter.second.data->size * count;
        }
    }
    else
    {
        data.name = typeJson;
        data.size = MapTypeToSize(typeJson, memberName);
    }
}

void Process(Inputs& out, nlohmann::json& inputsJson, nlohmann::json& root)
{
    out.clear();
    out.resize(inputsJson.size());
    int i = inputsJson.size() - 1; // seems like spirv-cross generate the input in the reversed order
    for (auto& inputJson : inputsJson)
    {
        Input input;
        input.location = inputJson.at("location");
        input.name = inputJson.at("name");
        Process(input.data, inputJson.at("type"), root, input.name);

        out[i] = std::move(input);
        i--;
    }
}

void Process(Outputs& out, nlohmann::json& outputsJson, nlohmann::json& root)
{
    out.clear();
    for (auto& outputJson : outputsJson)
    {
        Output output;
        output.location = outputJson.at("location");
        output.name = outputJson.at("name");
        Process(output.data, outputJson.at("type"), root, "");
        out.push_back(std::move(output));
    }
}

void Process(PushConstants& data, ShaderStage::Flag stage, nlohmann::json& pushConstantsJson, nlohmann::json& root)
{
    for (auto& pushConstantJson : pushConstantsJson)
    {
        auto iter = data.find(pushConstantJson.at("name"));
        if (iter == data.end())
        {
            PushConstant pushConstant;
            pushConstant.stages = stage;
            Process(pushConstant.data, pushConstantJson.at("type"), root, "");

            // push constant is a bit special here, spirv-cross reports the name in pushConstantJson as "pushConstant",
            // so we use the data's name instead
            pushConstant.name = pushConstant.data.name;
            data[pushConstant.name] = pushConstant;
        }
        else
        {
            iter->second.stages |= stage;
        }
    }
}

ShaderStage::Flag MapStage(const std::string& str)
{
    if (str == "vert")
        return ShaderStage::Vert;
    if (str == "frag")
        return ShaderStage::Frag;

    assert(0 && "Map stage failed");
    return ShaderStage::Vert;
}

SamplerFilterMode MapStringToSamplerFilterMode(std::string_view str)
{
    if (str.find("_point") != std::string_view::npos || str.find("_Point") != std::string_view::npos ||
        str.find("_POINT") != std::string_view::npos)
    {
        return SamplerFilterMode::Point;
    }

    if (str.find("_linear") != std::string_view::npos || str.find("_Linear") != std::string_view::npos ||
        str.find("_LINEAR") != std::string_view::npos)
    {
        return SamplerFilterMode::Linear;
    }

    return SamplerFilterMode::Point;
}

void Process(ShaderStageInfo& out, nlohmann::json& sr)
{
    out.name = sr.at("spvPath");
    assert(sr["entryPoints"].size() == 1);
    out.stage = MapStage(sr["entryPoints"][0]["mode"]);
    if (sr.contains("inputs"))
        Process(out.inputs, sr["inputs"], sr);
    if (sr.contains("outputs"))
        Process(out.outputs, sr["outputs"], sr);

    if (sr.contains("push_constants"))
        Process(out.pushConstants, out.stage, sr["push_constants"], sr);
    if (sr.contains("ubos"))
        Process(out.bindings, BindingType::UBO, out.stage, sr["ubos"], sr);
    if (sr.contains("ssbos"))
        Process(out.bindings, BindingType::SSBO, out.stage, sr["ssbos"], sr);
    if (sr.contains("textures"))
        Process(out.bindings, BindingType::Texture, out.stage, sr["textures"], sr);
    if (sr.contains("separate_images"))
        Process(out.bindings, BindingType::SeparateImage, out.stage, sr["separate_images"], sr);
    if (sr.contains("separate_samplers"))
        Process(out.bindings, BindingType::SeparateSampler, out.stage, sr["separate_samplers"], sr);
}

void Process(
    Bindings& out, BindingType type, ShaderStage::Flag stage, nlohmann::json& bindingsJson, nlohmann::json& root
)
{
    for (auto& bindingJson : bindingsJson)
    {
        std::string name = bindingJson.at("name");
        auto iter = out.find(name);
        if (iter == out.end())
        {
            auto& b = out[name];
            b.bindingNum = bindingJson.at("binding");
            b.setNum = bindingJson.at("set");
            b.type = type;
            switch (type)
            {
                case BindingType::UBO:
                    {
                        new (&b.binding.ubo) UBO();
                        Process(b.binding.ubo.data, bindingJson["type"], root, "");
                        break;
                    }
                case BindingType::SSBO:
                    {
                        new (&b.binding.ssbo) SSBO();
                        Process(b.binding.ssbo.data, bindingJson["type"], root, "");
                        break;
                    }
                case BindingType::Texture:
                    {
                        new (&b.binding.texture) Texture();
                        b.binding.texture.filter = MapStringToSamplerFilterMode(name);
                        b.binding.texture.type = MapStringToTextureType(bindingJson.value("type", "sampler2D"));
                        break;
                    }
                case BindingType::SeparateImage: new (&b.binding.separateImage) SeparateImage(); break;
                case BindingType::SeparateSampler: new (&b.binding.separateSampler) SeparateSampler(); break;
                default: assert(0 && "Not implemented");
            }
            b.count = bindingJson.value("array", std::vector<int>{1})[0];
            b.name = RemoveShaderNameConfigText(name);
            b.stages = stage;
        }
        else
        {
            iter->second.stages |= stage;
        }
    }
}

#define MergeHelper(member, TypeName)                                                                                  \
    for (auto& member : from.member##s)                                                                                \
    {                                                                                                                  \
        auto iter = to.bindings.find(member.name);                                                                     \
        if (iter == to.bindings.end())                                                                                 \
        {                                                                                                              \
            auto& b = to.bindings[member.name];                                                                        \
            b.type = BindingType::TypeName;                                                                            \
            b.stages = from.stage;                                                                                     \
            b.binding.member = member;                                                                                 \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            iter->second.stages |= from.stage;                                                                         \
        }                                                                                                              \
    }

void Merge(ShaderInfo& to, const ShaderStageInfo& from)
{
    switch (from.stage)
    {
        case ShaderStage::Vert: to.vertName = from.name; break;
        case ShaderStage::Frag: to.fragName = from.name; break;
        default: assert(0 && "Shader Stage not handled");
    }
    for (auto& binding : from.bindings)
    {
        auto iter = to.bindings.find(binding.first);
        if (iter == to.bindings.end())
        {
            iter = to.bindings.insert(std::make_pair(binding.first, binding.second)).first;
            to.descriptorSetBinidngMap[iter->second.setNum].bindings.insert(
                std::make_pair(binding.first, binding.second)
            );
        }
        iter->second.stages |= binding.second.stages;
    }

    for (auto& pushConstant : from.pushConstants)
    {
        auto iter = to.pushConstants.find(pushConstant.second.name);
        if (iter == to.pushConstants.end())
        {
            auto pushConstantPair = std::make_pair(pushConstant.first, pushConstant.second);
            iter = to.pushConstants.insert(std::move(pushConstantPair)).first;
        }
        iter->second.stages |= pushConstant.second.stages;
    }
}
} // namespace Utils
} // namespace Engine::Gfx::ShaderInfo
