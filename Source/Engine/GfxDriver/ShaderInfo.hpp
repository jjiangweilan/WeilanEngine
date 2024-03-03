#pragma once
#include <cinttypes>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "DescriptorSetSlot.hpp"
#include "GfxDriver/ResourceHandle.hpp"
#include "Libs/Ptr.hpp"

namespace Gfx::ShaderInfo
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
    Member(Member&& other)
        : name(std::exchange(other.name, "")), data(std::exchange(other.data, nullptr)), offset(other.offset),
          dimension(std::exchange(other.dimension, {}))
    {}

    Member(const Member& other)
        : name(other.name), data(MakeUnique<StructuredData>(*other.data)), offset(other.offset),
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
        return other.name == name && *data == *other.data && offset == other.offset && dimension == other.dimension;
    }

    std::string name = "";
    UniPtr<StructuredData> data = nullptr;
    uint32_t offset = 0;
    std::vector<uint8_t> dimension{};
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
    return first.name == second.name && first.type == second.type && first.size == second.size &&
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
    StructuredData data{};
};

struct SSBO
{
    StructuredData data{};
};

enum class SamplerFilterMode
{
    Linear,
    Point
};

struct Texture
{
    enum class Type
    {
        Tex2D,
        Tex3D,
        TexCube,
    };

    bool enableCompare;
    SamplerFilterMode filter;
    Type type;
};

struct SeparateImage
{};

struct StorageImage
{};

struct SeparateSampler
{
    bool enableCompare;
};

using Outputs = std::vector<Output>;
using Inputs = std::vector<Input>;
using UBOs = std::vector<UBO>;

namespace ShaderStage
{
// value copies from Vulkan VkShaderStageFlagBits
enum Flag
{
    Vert = 0x00000001,
    Frag = 0x00000010,
    Comp = 0x00000020,
};
} // namespace ShaderStage
using ShaderStageFlags = uint32_t;

enum class BindingType
{
    UBO,
    SSBO,
    Texture,
    StorageImage,
    SeparateImage,
    SeparateSampler
};

enum class TextureType
{
    Tex2D,
    TexCube
};

struct Binding
{
    Binding() = default;

    Binding& operator=(const Binding& other)
    {
        stages = other.stages;
        setNum = other.setNum;
        bindingNum = other.bindingNum;
        name = other.name;
        resourceHandle = other.resourceHandle;
        count = other.count;
        type = other.type;
        switch (other.type)
        {
            case BindingType::UBO:
                {
                    if (type != BindingType::UBO)
                    {
                        new (&binding.ubo) UBO();
                    }
                    binding.ubo = other.binding.ubo;
                    break;
                }
            case BindingType::StorageImage:
                {
                    if (type != BindingType::StorageImage)
                    {
                        new (&binding.storageImage) StorageImage();
                    }
                    binding.storageImage = other.binding.storageImage;
                    break;
                }
            case BindingType::SSBO:
                if (type != BindingType::SSBO)
                {
                    new (&binding.ssbo) SSBO();
                }
                binding.ssbo = other.binding.ssbo;
                break;
            case BindingType::Texture:
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
        return *this;
    }

    Binding(const Binding& other)
    {
        *this = other;
    }

    ~Binding()
    {
        switch (type)
        {
            case BindingType::UBO: binding.ubo.~UBO(); break;
            case BindingType::StorageImage: binding.storageImage.~StorageImage(); break;
            case BindingType::Texture: binding.texture.~Texture(); break;
            case BindingType::SeparateImage: binding.separateImage.~SeparateImage(); break;
            case BindingType::SeparateSampler: binding.separateSampler.~SeparateSampler(); break;
            case BindingType::SSBO: binding.ssbo.~SSBO(); break;
        }
    }

    BindingType type = BindingType::UBO;
    ShaderStageFlags stages = 0;
    uint32_t setNum;
    uint32_t bindingNum;
    std::string name;
    ResourceHandle resourceHandle; // used by render graph
    std::string actualName;
    uint8_t count = 1;
    union BindingUnion
    {
        BindingUnion() : ubo{} {}
        ~BindingUnion() {}
        Texture texture;
        StorageImage storageImage;
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
    std::string compName;
    std::unordered_map<SetNum, std::vector<Binding*>> descriptorSetBindingMap;
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
void Process(
    Bindings& out, BindingType type, ShaderStage::Flag stage, nlohmann::json& bindingsJson, nlohmann::json& root
);

void Merge(ShaderInfo& to, const ShaderStageInfo& from);
} // namespace Utils
} // namespace Gfx::ShaderInfo
