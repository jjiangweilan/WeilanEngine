#pragma once
#include <cinttypes>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <utility>

#include "Libs/Ptr.hpp"
#include "DescriptorSetSlot.hpp"
namespace Engine::Gfx::ShaderInfo
{
    enum class ShaderDataType
    {
        Float,
        Half,
        Vec4,
        Vec3,
        Vec2,
        Mat4,
        Mat3,
        Mat2,
        Structure
    };

    struct StructuredData;
    inline bool operator==(const StructuredData& first, const StructuredData& second);
    struct Member
    {
        Member(Member&& other) : 
            name(std::exchange(other.name, "")),
            data(std::exchange(other.data, nullptr)),
            offset(other.offset),
            dimension(std::exchange(other.dimension, {}))
            {}

        Member(const Member& other) :
            name(other.name),
            data(MakeUnique<StructuredData>(*other.data)),
            offset(other.offset),
            dimension(other.dimension){};

        Member& operator=(const Member& other)
        {
            name = other.name;
            data = MakeUnique<StructuredData>(*other.data);
            offset = other.offset;
            dimension = other.dimension;

            return *this;
        }


        Member() = default;
        ~Member() = default;
        bool operator==(const Member& other) const
        {
            return other.name == name &&
                *data == *other.data &&
                offset == other.offset &&
                dimension == other.dimension;
        }

        std::string name = "";
        UniPtr<StructuredData> data = nullptr;
        uint32_t offset = 0;
        std::vector<uint8_t> dimension {};
    };

    struct StructuredData
    {
        std::string name = "";
        ShaderDataType type = ShaderDataType::Vec4;
        uint32_t size = 0;
        std::unordered_map<std::string, Member> members;
    };

    bool operator==(const StructuredData& first, const StructuredData& second)
    {
        return first.name == second.name &&
            first.type == second.type &&
            first.size == second.size &&
            first.members == second.members;
    }

    struct Input
    {
        std::string name;
        uint32_t location;
        StructuredData data;
    };

    struct Output
    {
        std::string name;
        uint32_t location;
        StructuredData data;
    };

    struct UBO
    {
        StructuredData data {};
    };

    struct SSBO
    {
        StructuredData data {};
    };

    struct Texture
    {
    };

    struct SeparateImage
    {

    };

    struct SeparateSampler
    {

    };

    using Outputs = std::vector<Output>;
    using Inputs = std::vector<Input>;
    using UBOs = std::vector<UBO>;

    namespace ShaderStage
    {
        enum Flag
        {
            Vert = 0x1, Frag = 0x2
        };
    }
    using ShaderStageFlags = uint32_t;

    enum class BindingType
    {
        UBO, SSBO, Texture, SeparateImage, SeparateSampler
    };

    struct Binding
    {
        Binding() = default;

        Binding(const Binding& other) : 
            stages(other.stages),
            setNum(other.setNum),
            bindingNum(other.bindingNum),
            name(other.name),
            count(other.count)
         {
            switch (other.type) 
            {
                case BindingType::UBO : 
                {
                    if (type != BindingType::UBO)
                    {
                        new (&binding.ubo) UBO();
                    }
                    binding.ubo = other.binding.ubo;
                    break;
                }
                case BindingType::SSBO:
                    if (type != BindingType::SSBO)
                    {
                        new (&binding.ssbo) SSBO();
                    }
                    binding.ssbo = other.binding.ssbo;
                    break;
                case BindingType::Texture :
                {
                    if (type != BindingType::Texture)
                    {
                        new (&binding.texture) Texture();
                    }
                    binding.texture = other.binding.texture;
                    break;
                }
                case BindingType::SeparateImage:
                {
                    if (type != BindingType::SeparateImage)
                    {
                        new (&binding.separateImage) SeparateImage();
                    }
                    binding.separateImage = other.binding.separateImage;
                    break;
                }
                case BindingType::SeparateSampler:
                {
                    if (type != BindingType::SeparateSampler)
                    {
                        new (&binding.separateSampler) SeparateSampler();
                    }
                    binding.separateSampler = other.binding.separateSampler;
                    break;
                }
            }

            type = other.type;
         }

        ~Binding()
        {
            switch (type)
            {
                case BindingType::UBO:
                    binding.ubo.~UBO();
                    break;
                case BindingType::Texture:
                    binding.texture.~Texture();
                    break;
                case BindingType::SeparateImage:
                    binding.separateImage.~SeparateImage();
                    break;
                case BindingType::SeparateSampler:
                    binding.separateSampler.~SeparateSampler();
                    break;
                case BindingType::SSBO:
                    binding.ssbo.~SSBO();
                    break;
            }
        }

        BindingType type = BindingType::UBO;
        ShaderStageFlags stages = 0;
        uint32_t setNum;
        uint32_t bindingNum;
        std::string name;
        uint8_t count = 1;
        union BindingUnion
        {
            BindingUnion() : ubo{} {}
            ~BindingUnion() {}
            Texture texture;
            SeparateImage separateImage;
            SeparateSampler separateSampler;
            UBO ubo;
            SSBO ssbo;
        } binding;
    };

    struct PushConstant
    {
        std::string name;
        StructuredData data;
        ShaderStageFlags stages;
    };

    using Bindings = std::unordered_map<std::string, Binding>;
    using PushConstants = std::unordered_map<std::string, PushConstant>;

    struct ShaderStageInfo
    {
        std::string name;
        Inputs inputs;
        Outputs outputs;
        Bindings bindings;
        PushConstants pushConstants;
        ShaderStage::Flag stage;
    };

    struct DescriptorSetInfo
    {
        Bindings bindings;
    };

    using SetNum = uint32_t;

    struct ShaderInfo
    {
        std::string vertName;
        std::string fragName;
        std::unordered_map<SetNum, DescriptorSetInfo> descriptorSetBinidngMap;
        Bindings bindings;
        PushConstants pushConstants;
    };



namespace Utils
{
    void Process(ShaderStageInfo& out, nlohmann::json& shaderReflect);
    void Process(StructuredData& out, nlohmann::json& typeJson, nlohmann::json& root, const std::string& memberName);
    void Process(PushConstants& out, ShaderStage::Flag stage, nlohmann::json& typeJson, nlohmann::json& root);
    void Process(Inputs& out, nlohmann::json& inputsJson, nlohmann::json& root);
    void Process(Outputs& out, nlohmann::json& outputsJson, nlohmann::json& root);
    void Process(Bindings& out, BindingType type, ShaderStage::Flag stage, nlohmann::json& bindingsJson, nlohmann::json& root);

    void Merge(ShaderInfo& to, const ShaderStageInfo& from);
}
}
