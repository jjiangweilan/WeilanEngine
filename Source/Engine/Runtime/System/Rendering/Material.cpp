#include "Material.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/MiddleLayer/FrameContext.hpp"
#include "Engine/Runtime/System/Rendering/MaterialUploadManager.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <algorithm>

namespace
{
constexpr std::string_view SceneLitShaderName = "SceneLit";
constexpr std::string_view TreeSceneLitShaderName = "TreeSceneLit";

std::string ResolveShaderNameAlias(std::string_view shaderName)
{
    if (shaderName == "SceneLitSkinned")
        return ShaderLibrary::GetShaderName(Shaders::SceneLit);

    return std::string(shaderName);
}

} // namespace

DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");
TYPE_REFLECTION_MEMBER_VARIABLES(
    Material,
    TYPE_REFLECTION_MEM(Material, shaderName),
    TYPE_REFLECTION_MEM(Material, shaderInUse),
    TYPE_REFLECTION_MEM(Material, ubo),
    TYPE_REFLECTION_MEM(Material, textureValues),
    TYPE_REFLECTION_MEM(Material, textureSamplerIndices),
    TYPE_REFLECTION_MEM(Material, enabledFeatures),
    TYPE_REFLECTION_MEM(Material, overridePipelineConfig),
    TYPE_REFLECTION_MEM(Material, pipelineConfig)
)

Material::Material(std::string_view shaderName)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetShader(shaderName);
}

Material::Material(ObjPtr<Shader> shader)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetShaderNoProtection(shader);
}

Material::Material()
    : shaderInUse(nullptr), shaderResource(nullptr)
{
    shaderResource = GetGfxDriver()->CreateShaderResource();
    SetName("new material");
}

Material::~Material()
{
    UnregisterGPUMaterial();
};

void Material::Copy(const Material& src)
{
    SetName(src.GetName());
    ubo = src.ubo;
    shaderName = src.shaderName;
    shaderFeatures = src.shaderFeatures;
    shaderInUse = src.shaderInUse;
    pipelineConfig = src.pipelineConfig;
    overridePipelineConfig = src.overridePipelineConfig;
    targetDescriptorSet = src.targetDescriptorSet;
    textureValues = src.textureValues;
    textureSamplerIndices = src.textureSamplerIndices;
    textureImageViewOptions = src.textureImageViewOptions;
    bufferValues = src.bufferValues;
    enabledFeatures = src.enabledFeatures;
    gpuExtraData = src.gpuExtraData;

    if (shaderResource)
    {
        shaderResource->Clear();
        for (auto& [name, texture] : textureValues)
        {
            if (texture != nullptr)
            {
                auto optionIter = textureImageViewOptions.find(name);
                SetTextureInternal(name, texture, optionIter != textureImageViewOptions.end() ? optionIter->second : std::nullopt);
            }
        }
        for (auto& [name, buffer] : bufferValues)
        {
            if (buffer != nullptr)
                shaderResource->SetBuffer(name, buffer);
        }
    }

    uploadNeeded = true;
    needRequestNewShader = src.needRequestNewShader;
    MarkGPUMaterialUploadNeeded();
}

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    if (shaderResource != nullptr)
        shaderResource->Remove(param);
    MarkGPUMaterialUploadNeeded();
    SetDirty();
}

void Material::RawSetTexture(const std::string& param, const ObjPtr<Texture>& texture)
{
    auto iter = textureValues.find(param);
    bool same = false;
    if (iter != textureValues.end())
    {
        same = iter->second == texture;
    }

    if (same)
        return;

    textureValues[param] = texture;
    MarkGPUMaterialUploadNeeded();
}

void Material::SetTexture(
    const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    // if texture is nullptr, redirect to nullptr implementation
    if (texture == nullptr)
    {
        SetTexture(param, (std::nullptr_t) nullptr);
        return;
    }

    auto iter = textureValues.find(param);
    bool same = false;
    if (iter != textureValues.end())
    {
        same = iter->second.Get() == texture;
    }
    if (same)
    {
        auto optionIter = textureImageViewOptions.find(param);
        if (optionIter != textureImageViewOptions.end())
        {
            same = imageViewOption == optionIter->second;
        }
    }
    if (same)
        return;

    SetDirty();
    SetTextureInternal(param, texture, imageViewOption);
    MarkGPUMaterialUploadNeeded();
}

void Material::SetTextureInternal(
    const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    textureValues[param] = texture;
    textureImageViewOptions[param] = imageViewOption;

    if (shaderResource != nullptr)
    {
        if (imageViewOption.has_value())
        {
            shaderResource->SetImage(param, &texture->GetGfxImage()->GetImageView(*imageViewOption));
        }
        else
        {
            shaderResource->SetImage(param, texture->GetGfxImage());
        }
    }
}

void Material::SetBuffer(const std::string& name, Gfx::Buffer* buffer)
{
    bufferValues[name] = buffer;
    shaderResource->SetBuffer(name, buffer);
}

void Material::RebuildAllMaterials()
{
    auto materials = Object::GetObjectsOfType<Material>();
    for (Material* mat : materials)
    {
        if (mat->shaderResource)
        {
            mat->shaderResource->Clear();

            // copy to skip reset test
            auto texCopy = mat->textureValues;
            auto bufCopy = mat->bufferValues;
            mat->textureValues.clear();
            mat->bufferValues.clear();
            for (auto& kv : texCopy)
            {
                if (kv.second != nullptr)
                    mat->SetTexture(kv.first, kv.second);
            }
            for (auto& kv : bufCopy)
            {
                if (kv.second != nullptr)
                    mat->SetBuffer(kv.first, kv.second);
            }
            mat->ubo.buffer = nullptr;
            mat->ubo.dirty = true;
            mat->uploadNeeded = true;
        }
    }
}

void Material::SetTexture(
    const std::string& param, Gfx::Image* image, std::optional<Gfx::ImageViewOption> imageViewOption
)
{
    if (shaderResource != nullptr)
    {
        if (imageViewOption.has_value())
            shaderResource->SetImage(param, &image->GetImageView(*imageViewOption));
        else
            shaderResource->SetImage(param, image);
    }
}

void Material::SetMatrix(const std::string& name, const glm::mat4& value)
{
    SetMatrix("", name, value);
}

void Material::SetFloat(const std::string& name, float value)
{
    SetFloat("", name, value);
}

void Material::SetVector(const std::string& name, const glm::vec4& value)
{
    SetVector("", name, value);
}

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    auto iter = ubo.matrices.find(member);
    if (iter == ubo.matrices.end() || iter->second != value)
    {
        ubo.matrices[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        SetDirty();
    }
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    auto iter = ubo.floats.find(member);
    if (iter == ubo.floats.end() || iter->second != value)
    {
        ubo.floats[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        MarkGPUMaterialUploadNeeded();
        SetDirty();
    }
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    auto iter = ubo.vectors.find(member);
    if (iter == ubo.vectors.end() || iter->second != value)
    {
        ubo.vectors[member] = value;
        ubo.dirty = true;
        uploadNeeded = true;
        MarkGPUMaterialUploadNeeded();
        SetDirty();
    }
}

glm::mat4 Material::GetMatrix(const std::string& param, const std::string& member)
{
    auto memIter = ubo.matrices.find(member);
    if (memIter != ubo.matrices.end())
    {
        return memIter->second;
    }
    return glm::mat4(0);
}

glm::vec4 Material::GetVector(const std::string& param, const std::string& member)
{
    auto memIter = ubo.vectors.find(member);
    if (memIter != ubo.vectors.end())
    {
        return memIter->second;
    }
    return glm::vec4(0);
}

Texture* Material::GetTexture(const std::string& param)
{
    auto iter = textureValues.find(param);
    if (iter != textureValues.end())
    {
        return iter->second;
    }

    return nullptr;
}

float Material::GetFloat(const std::string& param, const std::string& member)
{
    auto memIter = ubo.floats.find(member);
    if (memIter != ubo.floats.end())
    {
        return memIter->second;
    }
    return 0;
}

void Material::SetShader(Shaders shader)
{
    SetShader(ShaderLibrary::ShaderNameMap[(int)shader]);
}

void Material::SetShader(Shader* shader)
{
    if (this->shaderInUse.Get() != shader)
    {
        needRequestNewShader = false;
        this->shaderName = shader->GetName();
        SetShaderNoProtection(shader);
        SetDirty();
    }
}

void Material::SetShader(std::string_view shaderName)
{
    std::string resolvedShaderName = ResolveShaderNameAlias(shaderName);
    if (shaderInUse == nullptr || this->shaderName != resolvedShaderName)
    {
        auto shaderFeatures = &ShaderLibrary::QueryShaderFeatures(resolvedShaderName.c_str());

        if (shaderFeatures)
        {
            auto perm = shaderFeatures->GetPermutation(enabledFeatures);
            auto shader = ShaderLibrary::GetShader(resolvedShaderName.c_str(), perm);

            if (shader == nullptr)
                return;

            needRequestNewShader = false;
            this->shaderFeatures = shaderFeatures;
            this->shaderName = resolvedShaderName;
            SetShaderNoProtection(shader);
            SetDirty();
        }
    }
}

void Material::SetShaderNoProtection(ObjPtr<Shader> shaderProgram)
{
    if (shaderProgram->GetName() == SceneLitShaderName || shaderProgram->GetName() == TreeSceneLitShaderName)
        InitializeSceneLitDefaults();
    if (shaderProgram->GetName() == TreeSceneLitShaderName)
        InitializeTreeWindDefaults();

    this->shaderInUse = shaderProgram;
    uploadNeeded = true;
    pipelineConfig = shaderInUse->GetShaderProgram()->GetDefaultPipelineConfig();
}

void Material::InitializeSceneLitDefaults()
{
    if (!ubo.floats.contains("shadowIntensityScale"))
        SetFloat("shadowIntensityScale", 1.0f);
}

void Material::InitializeTreeWindDefaults()
{
    if (!ubo.vectors.contains("windDirection"))
        SetVector("windDirection", {1.0f, 0.0f, 0.0f, 0.0f});
    if (!ubo.floats.contains("windStrength"))
        SetFloat("windStrength", 0.15f);
    if (!ubo.floats.contains("windSpeed"))
        SetFloat("windSpeed", 1.0f);
    if (!ubo.floats.contains("windFrequency"))
        SetFloat("windFrequency", 0.25f);
    if (!ubo.floats.contains("windBaseHeight"))
        SetFloat("windBaseHeight", 0.0f);
    if (!ubo.floats.contains("windBendHeight"))
        SetFloat("windBendHeight", 4.0f);
}

void Material::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shader", shaderInUse);
    s->Serialize("ubo", ubo);
    s->Serialize("textureValues", textureValues);
    s->Serialize("textureSamplerIndices", textureSamplerIndices);
    std::vector<std::string> enabledFeatureVec(enabledFeatures.begin(), enabledFeatures.end());
    s->Serialize("enabledFeature", enabledFeatureVec);
    s->Serialize("overridePipelineConfig", overridePipelineConfig);
    s->Serialize("pipelineConfig", overridePipelineConfig ? pipelineConfig.ToJson() : nlohmann::json());
    SERIALIZE(s, shaderName);
}

std::unique_ptr<Asset> Material::Clone()
{
    return std::make_unique<Material>(*this);
}

Gfx::ShaderResource* Material::ValidateGetShaderResource()
{
    if (shaderInUse == nullptr)
    {
        spdlog::warn("Shader is not set in material");
        return nullptr;
    }

    if (uploadNeeded)
    {
        UploadDataToGPU(shaderInUse->GetShaderProgram());
    }
    return shaderResource.get();
}

Gfx::ShaderProgram* Material::GetShaderProgram()
{
    if (needRequestNewShader)
    {
        if (!shaderFeatures)
        {
            shaderFeatures = &ShaderLibrary::QueryShaderFeatures(shaderName.data());
        }
        shaderInUse = ShaderLibrary::GetShader(shaderName.data(), shaderFeatures->GetPermutation(enabledFeatures));
        needRequestNewShader = false;
    }

    if (shaderInUse)
        return shaderInUse->GetShaderProgram();

    return nullptr;
}

void Material::EnableFeature(const std::string& name)
{
    if (!enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.emplace(name);
        SetDirty();
    }
}

void Material::DisableFeature(const std::string& name)
{
    if (enabledFeatures.contains(name))
    {
        needRequestNewShader = true;
        enabledFeatures.erase(name);
        SetDirty();
    }
}

void Material::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    // s->Deserialize("shader", shader);
    s->Deserialize("ubo", ubo);
    s->Deserialize("textureValues", textureValues);
    s->Deserialize("textureSamplerIndices", textureSamplerIndices);
    std::vector<std::string> enabledFeatureVec;
    s->Deserialize("enabledFeature", enabledFeatureVec);
    for (auto& f : enabledFeatureVec)
    {
        EnableFeature(f);
    }
    nlohmann::json pipelineConfigJson;
    bool deserializedOverridePipelineConfig = false;
    s->Deserialize("pipelineConfig", pipelineConfigJson);
    s->Deserialize("overridePipelineConfig", deserializedOverridePipelineConfig);
    DESERIALIZE(s, shaderName);
    if (!shaderName.empty())
    {
        SetShader(shaderName);
    }

    overridePipelineConfig = deserializedOverridePipelineConfig;
    if (overridePipelineConfig)
    {
        pipelineConfig = Gfx::PipelineConfig::FromJson(pipelineConfigJson);
    }
}

void Material::OnLoaded()
{
    for (auto& kv : textureValues)
    {
        if (kv.second != nullptr)
        {
            SetTextureInternal(kv.first, kv.second, std::nullopt);
        }
    }

    RegisterGPUMaterial();
}

void Material::UBO::Serialize(Serializer* ser) const
{
    ser->Serialize("floats", floats);
    ser->Serialize("vectors", vectors);
    ser->Serialize("matrices", matrices);
}

void Material::UBO::Deserialize(Serializer* ser)
{
    ser->Deserialize("floats", floats);
    ser->Deserialize("vectors", vectors);
    ser->Deserialize("matrices", matrices);
    dirty = true;
}

void Material::UploadDataToGPU(Gfx::ShaderProgram* shaderProgram)
{
    if (!shaderProgram)
        return;

    RegisterGPUMaterial();

    uploadNeeded = false;

    if (ubo.dirty)
    {
        ubo.dirty = false;
        const auto& pipelineInfo = shaderProgram->GetShaderInfo();
        auto descriptorSet = pipelineInfo.GetDescriptorSet(targetDescriptorSet);
        if (descriptorSet == nullptr)
            return;

        auto binding = descriptorSet->GetBinding(0);
        if (binding != nullptr && binding->descriptorType == Gfx::DescriptorType::UniformBuffer)
        {
            // Create the buffer
            if (ubo.buffer == nullptr)
            {
                size_t size = binding->byteSize;
                std::unique_ptr<Gfx::Buffer> buffer = GetGfxDriver()->CreateBuffer({
                    .usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
                    .size = size,
                    .visibleInCPU = false,
                    .debugName = "Material Uniform Buffer",
                });

                auto uboBindingName = pipelineInfo.GetDescriptorSet(targetDescriptorSet)->name;
                shaderResource->SetBuffer(uboBindingName, buffer.get());
                ubo.buffer = std::move(buffer);
            }

            auto bufSize = binding->byteSize;

            auto [tempBuf, tempUploadDataHandle] = GetSharedStackAllocator().ScopedAllocate<unsigned char>(bufSize);
            memset(tempBuf, 0, bufSize);

            ASSERT(binding->descriptorType == Gfx::DescriptorType::UniformBuffer && "UBO should be a structure");
            for (auto& member : binding->bufferMembers)
            {
                WriteParameterDataToBuffer(pipelineInfo, member, tempBuf, bufSize);
            }
            GetGfxDriver()->UploadBuffer(*ubo.buffer, tempBuf, bufSize, 0);
        }
    }
}

void Material::WriteParameterDataToBuffer(
    const Gfx::ShaderPipelineInfo& pipelineInfo,
    const Gfx::ShaderPipelineInfo::BufferMember& bufferDataDescription,
    uint8_t* buf,
    size_t bufSize
)
{
    size_t offset = bufferDataDescription.offset;
    ASSERT(!bufferDataDescription.IsArray());

    if (bufferDataDescription.IsVector())
    {
#define COPY_TO_BUFFER(type)                                                            \
    auto iter = ubo.vectors.find(bufferDataDescription.name);                           \
    if (iter != ubo.vectors.end())                                                      \
    {                                                                                   \
        if (bufferDataDescription.rowCount == 3 || bufferDataDescription.rowCount == 4) \
        {                                                                               \
            ASSERT(offset + sizeof(glm::type##4) <= bufSize);                           \
            *((glm::type##4 *)(buf + offset)) = iter->second;                           \
        }                                                                               \
        else if (bufferDataDescription.rowCount == 2)                                   \
        {                                                                               \
            ASSERT(offset + sizeof(glm::type##2) <= bufSize);                           \
            *((glm::type##2 *)(buf + offset)) = glm::type##2(iter->second);             \
        }                                                                               \
    }

        if (bufferDataDescription.type == Gfx::ShaderPipelineInfo::MemberDataType::Float)
        {
            COPY_TO_BUFFER(vec)
        }
        else if (bufferDataDescription.type == Gfx::ShaderPipelineInfo::MemberDataType::Int)
        {
            COPY_TO_BUFFER(ivec)
        }
        else if (bufferDataDescription.type == Gfx::ShaderPipelineInfo::MemberDataType::UInt)
        {
            COPY_TO_BUFFER(uvec)
        }
    }
    else if (bufferDataDescription.IsMatrix())
    {
        auto iter = ubo.matrices.find(bufferDataDescription.name);
        if (iter != ubo.matrices.end())
        {
            ASSERT(bufferDataDescription.rowCount == 4 && bufferDataDescription.columnCount == 4);
            {
                ASSERT(offset + sizeof(glm::mat4) <= bufSize);
                *((glm::mat4*)(buf + offset)) = iter->second;
            }
        }
    }
    else // scatter type
    {
        switch (bufferDataDescription.type)
        {
            case Gfx::ShaderPipelineInfo::MemberDataType::Float:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(float) <= bufSize);
                        *((float*)(buf + offset)) = iter->second;
                    }
                    break;
                }
            case Gfx::ShaderPipelineInfo::MemberDataType::UInt:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(uint32_t) <= bufSize);
                        *((uint32_t*)(buf + offset)) = (uint32_t)iter->second;
                    }
                    break;
                }
            case Gfx::ShaderPipelineInfo::MemberDataType::Int:
                {
                    auto iter = ubo.floats.find(bufferDataDescription.name);
                    if (iter != ubo.floats.end())
                    {
                        ASSERT(offset + sizeof(int32_t) <= bufSize);
                        *((int32_t*)(buf + offset)) = (int32_t)iter->second;
                    }
                    break;
                }
            case Gfx::ShaderPipelineInfo::MemberDataType::Structure:
                {
                    ASSERT(false && "Nested Structure not supported in material");
                    break;
                }
            default: ASSERT(0 && "Not Handled"); break;
        }
    }
}

const Gfx::PipelineConfig& Material::GetPipelineConfig()
{
    if (overridePipelineConfig || shaderInUse == nullptr)
        return pipelineConfig;
    else
    {
        Shader* s = shaderInUse;
        return s->GetShaderProgram()->GetDefaultPipelineConfig();
    }
}

int Material::GetSet(Gfx::DescriptorSetSemantics semantics) const
{
    if (shaderInUse)
    {
        auto set = shaderInUse->GetShaderProgram()->GetShaderInfo().GetDescriptorSet(semantics);
        if (set)
            return set->setNum;
    }

    return -1;
}

int Material::GetSet(const std::string& name) const
{
    if (shaderInUse)
    {
        auto set = shaderInUse->GetShaderProgram()->GetShaderInfo().GetDescriptorSet(name);
        if (set)
            return set->setNum;
    }

    return -1;
}

void Material::CopyProperties(Material& other)
{
    bool gpuMaterialDirty = false;
    if (ubo.floats != other.ubo.floats)
    {
        ubo.floats = other.ubo.floats;
        ubo.dirty = true;
        uploadNeeded = true;
        gpuMaterialDirty = true;
    }

    if (ubo.vectors != other.ubo.vectors)
    {
        ubo.vectors = other.ubo.vectors;
        ubo.dirty = true;
        uploadNeeded = true;
        gpuMaterialDirty = true;
    }

    if (ubo.matrices != other.ubo.matrices)
    {
        ubo.matrices = other.ubo.matrices;
        ubo.dirty = true;
        uploadNeeded = true;
    }

    if (gpuMaterialDirty)
        MarkGPUMaterialUploadNeeded();
}

void Material::SetTextureSamplerIndex(const std::string& bindingName, uint32_t samplerIndex)
{
    auto iter = textureSamplerIndices.find(bindingName);
    if (iter != textureSamplerIndices.end() && iter->second == samplerIndex)
        return;

    textureSamplerIndices[bindingName] = samplerIndex;
    MarkGPUMaterialUploadNeeded();
}

Rendering::GpuMaterial Material::BuildGPUMaterialData() const
{
    Rendering::GpuMaterial data{};
    auto getVector = [this](const std::string& name) -> glm::vec4
    {
        auto it = ubo.vectors.find(name);
        if (it != ubo.vectors.end())
            return it->second;
        return glm::vec4(0);
    };
    auto getFloat = [this](const std::string& name) -> float
    {
        auto it = ubo.floats.find(name);
        if (it != ubo.floats.end())
            return it->second;
        return 0.0f;
    };

    data.baseColorFactor = getVector("baseColorFactor");
    data.emissive = getVector("emissive");
    const glm::vec4 windDirection = getVector("windDirection");
    data.windDirectionStrengthSpeed = {
        windDirection.x,
        windDirection.y,
        getFloat("windStrength"),
        getFloat("windSpeed")
    };
    data.windFrequencyHeights = {
        getFloat("windFrequency"),
        getFloat("windBaseHeight"),
        getFloat("windBendHeight"),
        0.0f
    };
    data.roughness = getFloat("roughness");
    data.metallic = getFloat("metallic");
    data.alphaCutoff = getFloat("alphaCutoff");
    data.shadowIntensityScale = getFloat("shadowIntensityScale");
    data.baseColorTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.normalMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.metallicRoughnessTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.emissiveMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.extraMaterialData = Rendering::InvalidTextureIndex;
    data.shaderHash = 0;

    auto getTexAndSamplerIndex = [&](const std::string& name) -> glm::uvec2
    {
        auto it = textureValues.find(name);
        if (it != textureValues.end() && it->second != nullptr)
        {
            auto handle = it->second->GetGPUTextureHandle();
            if (handle != static_cast<Rendering::GPUTextureHandle>(-1))
            {
                uint32_t samplerIdx = 1; // default: Linear+Repeat
                auto sit = textureSamplerIndices.find(name);
                if (sit != textureSamplerIndices.end())
                    samplerIdx = sit->second;
                return glm::uvec2(static_cast<uint32_t>(handle), samplerIdx);
            }
        }
        return glm::uvec2(Rendering::InvalidTextureIndex, 1);
    };

    data.baseColorTexIndex = getTexAndSamplerIndex("baseColorTex");
    data.normalMapTexIndex = getTexAndSamplerIndex("normalMap");
    data.metallicRoughnessTexIndex = getTexAndSamplerIndex("metallicRoughnessMap");
    data.emissiveMapTexIndex = getTexAndSamplerIndex("emissiveMap");

    return data;
}

void Material::SetGPUDrivenExtraData(std::span<const uint8_t> data)
{
    if (gpuExtraData.size() == data.size() && std::equal(gpuExtraData.begin(), gpuExtraData.end(), data.begin()))
        return;

    gpuExtraData.assign(data.begin(), data.end());
    MarkGPUMaterialUploadNeeded();
}

void Material::RegisterGPUMaterial()
{
    if (gpuMaterialHandle != Rendering::InvalidGPUHandle)
        return;

    auto* gpuDrivenManager = Rendering::GPUDrivenManager::TryGetInstance();
    if (gpuDrivenManager == nullptr)
        return;

    gpuMaterialHandle = gpuDrivenManager->RegisterMaterial(BuildGPUMaterialData(), gpuExtraData);
    gpuMaterialUploadNeeded = false;
    MaterialUploadManager::Instance().RemovePendingUpload(this);
}

void Material::UnregisterGPUMaterial()
{
    MaterialUploadManager::Instance().RemovePendingUpload(this);
    gpuMaterialUploadNeeded = false;

    if (gpuMaterialHandle == Rendering::InvalidGPUHandle)
        return;

    auto* gpuDrivenManager = Rendering::GPUDrivenManager::TryGetInstance();
    if (gpuDrivenManager != nullptr)
    {
        gpuDrivenManager->UnregisterMaterial(gpuMaterialHandle);
    }
    gpuMaterialHandle = Rendering::InvalidGPUHandle;
}

void Material::UpdateGPUMaterialData()
{
    if (gpuMaterialHandle == Rendering::InvalidGPUHandle)
        return;

    auto* gpuDrivenManager = Rendering::GPUDrivenManager::TryGetInstance();
    if (gpuDrivenManager == nullptr)
        return;

    gpuDrivenManager->UpdateMaterial(gpuMaterialHandle, BuildGPUMaterialData(), gpuExtraData);
    gpuMaterialUploadNeeded = false;
    MaterialUploadManager::Instance().RemovePendingUpload(this);
}

void Material::MarkGPUMaterialUploadNeeded()
{
    gpuMaterialUploadNeeded = true;
    if (gpuMaterialHandle != Rendering::InvalidGPUHandle)
        MaterialUploadManager::Instance().AddPendingUpload(this);
}
