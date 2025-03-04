#pragma once
#include "GfxDriver/ShaderConfig.hpp"
#include "Libs/EnumFlags.hpp"
#include "ResourceHandle.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <sstream>

namespace Gfx
{
// replaced by ShaderPorgramCreateInfo. now CompiledSpv only used in importing process
struct CompiledSpv
{
    std::vector<uint32_t> vertSpv;
    std::vector<uint32_t> vertSpv_noOp;
    std::vector<uint32_t> fragSpv;
    std::vector<uint32_t> fragSpv_noOp;

    std::vector<uint32_t> compSpv;
    std::vector<uint32_t> compSpv_noOp;
};

struct ShaderProgramCreateInfoVertex
{
    bool vertInterleaved = false;
    struct InputAttribute
    {
        int channelByteSize = 32;
    };
    std::vector<InputAttribute> inputAttributes;
};

struct PipelineInfo
{
    enum class MemberDataType
    {
        Bool,
        Float,
        UInt,
        Int,
        Structure,
    };

    struct BufferMember
    {
        std::string name = "";
        MemberDataType type = MemberDataType::Float;
        uint32_t columnCount = 1;
        uint32_t rowCount = 1; // as element count when type is a Vector
        uint32_t offset = 0;   // byte offset in it's containning struct
        uint32_t count = 0;    // array
        uint32_t byteSize = 0;
        bool IsElement() const { return count == 0 && rowCount == 1 && columnCount == 1; }
        bool IsArray() const { return count > 0; }
        bool IsVector() const { return rowCount > 1 && columnCount == 1; }
        bool IsMatrix() const { return rowCount > 1 && columnCount > 1; }
    };

    struct Binding
    {
        std::string name = "";
        ShaderBindingHandle shaderBindingHandle = {}; // hash of name
        uint32_t bindingNum = 0;
        uint32_t descriptorCount = 0;
        ShaderStageFlags stages = ShaderStage::None;

        DescriptorType descriptorType = DescriptorType::Invalid;
        TextureType textureType = TextureType::Invalid;

        std::vector<BufferMember> bufferMembers = {};
        uint32_t byteSize = 0;

        int samplerIndex = -1;
    };

    struct VertexAttribute
    {
        std::string name = ""; // not hashed
        std::string semanticName = "";
        int semanticIndex = 0;
        int location;
        int size;
        GfxFormat format;
    };

    struct FragmentOutput
    {
        GfxFormat format;
        int location;
    };

    struct SamplerConfig
    {
        SamplerAddressMode addressModeU;
        SamplerAddressMode addressModeV;
        SamplerAddressMode addressModeW;
        SamplerMipmapMode mipmapMode;
        FilterMode minFilter;
        FilterMode magFilter;
        bool anisotropic;
        bool enbaleCompare;
    };

    struct DescriptorSet
    {
        std::string name;
        int setNum = 0;
        std::vector<Binding> bindings = {};
        std::unordered_map<std::string, Binding*> nameToBinding = {};
        std::vector<SamplerConfig> samplerConfigs = {};

        const Binding* GetBinding(std::string_view name) const
        {
            for (auto& b : bindings)
            {
                if (std::strcmp(b.name.data(), name.data()) == 0)
                {
                    return &b;
                }
            }

            return nullptr;
        }

        int AddSamplerConfig(const SamplerConfig& config)
        {
            samplerConfigs.push_back(config);
            return samplerConfigs.size() - 1;
        }

        void UpdateBindingIndex()
        {
            for (auto& b : bindings)
            {
                nameToBinding[b.name] = &b;
            }
        }
    };

    struct PushConstant
    {
        uint32_t size = 0;
        ShaderStageFlags stages = ShaderStage::None;
    };


    std::string name = "";
    std::string vertexShaderName = "";
    std::string fragmentShaderName = "";
    std::string computeShaderName = "";

    // this just means the position is not interleaved, but all the other attributes are interleaved
    bool isVertexInterleaved = false;
    std::vector<VertexAttribute> vertexInputs = {};
    std::vector<FragmentOutput> fragmentOutputs = {};
    std::vector<DescriptorSet> descriptorSets = {};
    std::vector<PushConstant> pushConstants = {};

    const DescriptorSet* GetDescriptorSet(const std::string& name) const
    {
        for (int i = 0; i < descriptorSets.size(); ++i)
        {
            if (descriptorSets[i].name == name)
            {
                return &descriptorSets[i];
            }
        }

        return nullptr;
    }

    std::string DumpInformation() const
    {
        std::stringstream ss;
        ss << "name:" << name << std::endl;
        ss << "descriptor sets" << std::endl;
        for (const auto& set : descriptorSets)
        {
            ss << "\tset " << set.setNum << std::endl;
            for (const auto& binding : set.bindings)
            {
                ss << "\t\t"
                   << "binding " << binding.bindingNum << ":" << binding.name << " "
                   << "size:" << binding.byteSize << std::endl;
                for (auto& member : binding.bufferMembers)
                {
                    ss << "\t\t\t name:" << member.name << " size:" << member.byteSize << std::endl;
                }
            }
        }

        return ss.str();
    }
};

struct PipelineCreateInfo
{
    std::vector<uint8_t> vertSpv = {};
    std::vector<uint8_t> fragSpv = {};
    std::vector<uint8_t> computeSpv = {};

    PipelineInfo pipelineInfo = {};
    PipelineConfig defaultConfig = {};
};
} // namespace Gfx
