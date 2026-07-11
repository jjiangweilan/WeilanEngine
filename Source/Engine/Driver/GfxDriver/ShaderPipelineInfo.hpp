#pragma once
#include "ResourceHandle.hpp"
#include "ShaderConfig.hpp"
#include "ShaderPipelineInfo.hpp"
#include "VertexAttributes.hpp"
#include <cinttypes>
#include <string>
#include <vector>

namespace Gfx
{
enum class DescriptorSetSemantics
{
    Global,
    Pass,
    Material,
    Object,
};

enum class ShaderDynamicState : uint32_t
{
    None = 0,
    DepthBiasEnable = 1 << 1,
    DepthBias = 1 << 2,
};
ENUM_FLAGS(ShaderDynamicState, uint32_t);
ShaderDynamicState StringToShaderDynamicState(const std::string& str);

struct ShaderPipelineInfo
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
        std::vector<std::string> attributes;
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
        bool isTextureArray = false;
        bool isVariableDescriptorCount = false;

        std::vector<BufferMember> bufferMembers = {};
        uint32_t byteSize = 0;

        int samplerIndex = -1;
    };

    struct VertexAttribute
    {
        std::string name = ""; // not hashed
        VertexAttributeSemantics semanticName = VertexAttributeSemantics::Position;
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
        bool enableCompare;
    };

    struct DescriptorSet
    {
        std::string name;
        int setNum = 0;
        DescriptorSetSemantics semantics = Gfx::DescriptorSetSemantics::Global;
        std::vector<Binding> bindings = {};
        std::unordered_map<std::string, Binding*> nameToBinding = {};
        std::vector<SamplerConfig> samplerConfigs = {};

        const Binding* GetBinding(int bindingIdx) const
        {
            if (bindingIdx >= 0 && bindingIdx < bindings.size())
            {
                return &bindings[bindingIdx];
            }

            return nullptr;
        }

        size_t GetBindingCount() const { return bindings.size(); }

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

    ShaderDynamicStateFlags shaderDynamicStateFlags = ShaderDynamicState::None;

    std::vector<BufferMember> uiPropertySchema;

    const DescriptorSet* GetDescriptorSet(DescriptorSetSemantics semantics) const
    {
        for (int i = 0; i < descriptorSets.size(); ++i)
        {
            if (descriptorSets[i].semantics == semantics)
            {
                return &descriptorSets[i];
            }
        }

        return nullptr;
    }

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
} // namespace Gfx
