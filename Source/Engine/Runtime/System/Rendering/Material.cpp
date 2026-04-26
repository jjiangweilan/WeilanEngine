#include "Material.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include "Engine/Driver/GfxDriver/ShaderResource.hpp"
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/MiddleLayer/FrameContext.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");
TYPE_REFLECTION_MEMBER_VARIABLES(
    Material,
    TYPE_REFLECTION_MEM(Material, shaderName),
    TYPE_REFLECTION_MEM(Material, shaderInUse),
    TYPE_REFLECTION_MEM(Material, ubo),
    TYPE_REFLECTION_MEM(Material, textureValues),
    TYPE_REFLECTION_MEM(Material, textureSamplerIndices),
    TYPE_REFLECTION_MEM(Material, enabledFeatures),
    TYPE_REFLECTION_MEM(Material, overrideShaderConfig),
    TYPE_REFLECTION_MEM(Material, shaderConfig)
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
    ubo = src.ubo;
    shaderName = src.shaderName;
    shaderFeatures = src.shaderFeatures;
    shaderInUse = src.shaderInUse;
    shaderConfig = src.shaderConfig;
    overrideShaderConfig = src.overrideShaderConfig;
    textureValues = src.textureValues;
    textureImageViewOptions = src.textureImageViewOptions;
    bufferValues = src.bufferValues;
    enabledFeatures = src.enabledFeatures;

    uploadNeeded = true;
    needRequestNewShader = src.needRequestNewShader;
}

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    if (shaderResource != nullptr)
        shaderResource->Remove(param);
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
    if (shaderInUse == nullptr || this->shaderName != shaderName)
    {
        auto shaderFeatures = &ShaderLibrary::QueryShaderFeatures(shaderName.data());

        if (shaderFeatures)
        {
            auto perm = shaderFeatures->GetPermutation(enabledFeatures);
            auto shader = ShaderLibrary::GetShader(shaderName.data(), perm);

            if (shader == nullptr)
                return;

            needRequestNewShader = false;
            this->shaderFeatures = shaderFeatures;
            this->shaderName = shaderName;
            SetShaderNoProtection(shader);
            SetDirty();
        }
    }
}

void Material::SetShaderNoProtection(ObjPtr<Shader> shaderProgram)
{
    this->shaderInUse = shaderProgram;
    uploadNeeded = true;
    shaderConfig = shaderInUse->GetShaderProgram()->GetDefaultShaderConfig();
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
    s->Serialize("overrideShaderConfig", overrideShaderConfig);
    s->Serialize("shaderConfig", overrideShaderConfig ? shaderConfig.ToJson() : nlohmann::json());
    SERIALIZE(s, shaderName);
}

std::unique_ptr<Asset> Material::Clone()
{
    return nullptr;
    // return std::unique_ptr<Material>(new Material(*this));
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
    nlohmann::json shaderConfigJson;
    s->Deserialize("shaderConfig", shaderConfigJson);
    s->Deserialize("overrideShaderConfig", overrideShaderConfig);
    DESERIALIZE(s, shaderName);
    if (!shaderName.empty())
    {
        SetShader(shaderName);
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
        ASSERT(bufferDataDescription.type == Gfx::ShaderPipelineInfo::MemberDataType::Float);
        {
            auto iter = ubo.vectors.find(bufferDataDescription.name);
            if (iter != ubo.vectors.end())
            {
                if (bufferDataDescription.rowCount == 3 || bufferDataDescription.rowCount == 4)
                {
                    ASSERT(offset + sizeof(glm::vec4) <= bufSize);
                    *((glm::vec4*)(buf + offset)) = iter->second;
                }
                else if (bufferDataDescription.rowCount == 2)
                {
                    ASSERT(offset + sizeof(glm::vec2) <= bufSize);
                    *((glm::vec2*)(buf + offset)) = glm::vec2(iter->second);
                }
            }
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

const Gfx::PipelineConfig& Material::GetShaderConfig()
{
    if (overrideShaderConfig || shaderInUse == nullptr)
        return shaderConfig;
    else
    {
        Shader* s = shaderInUse;
        return s->GetShaderProgram()->GetDefaultShaderConfig();
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
    if (ubo.floats != other.ubo.floats)
    {
        ubo.floats = other.ubo.floats;
        ubo.dirty = true;
        uploadNeeded = true;
    }

    if (ubo.vectors != other.ubo.vectors)
    {
        ubo.vectors = other.ubo.vectors;
        ubo.dirty = true;
        uploadNeeded = true;
    }

    if (ubo.matrices != other.ubo.matrices)
    {
        ubo.matrices = other.ubo.matrices;
        ubo.dirty = true;
        uploadNeeded = true;
    }
}

void Material::SetTextureSamplerIndex(const std::string& bindingName, uint32_t samplerIndex)
{
    textureSamplerIndices[bindingName] = samplerIndex;
}

void Material::RegisterGPUMaterial()
{
    if (gpuMaterialHandle != Rendering::InvalidGPUHandle)
        return;

    Rendering::GpuMaterial data{};
    data.baseColorFactor = GetVector("", "baseColorFactor");
    data.emissive = GetVector("", "emissive");
    data.roughness = GetFloat("", "roughness");
    data.metallic = GetFloat("", "metallic");
    data.alphaCutoff = GetFloat("", "alphaCutoff");
    data.baseColorTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.normalMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.metallicRoughnessTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.emissiveMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
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

    gpuMaterialHandle = Rendering::GPUDrivenManager::Instance().RegisterMaterial(data);
}

void Material::UnregisterGPUMaterial()
{
    if (gpuMaterialHandle == Rendering::InvalidGPUHandle)
        return;

    Rendering::GPUDrivenManager::Instance().UnregisterMaterial(gpuMaterialHandle);
    gpuMaterialHandle = Rendering::InvalidGPUHandle;
}

void Material::UpdateGPUMaterialData()
{
    if (gpuMaterialHandle == Rendering::InvalidGPUHandle)
        return;

    Rendering::GpuMaterial data{};
    data.baseColorFactor = GetVector("", "baseColorFactor");
    data.emissive = GetVector("", "emissive");
    data.roughness = GetFloat("", "roughness");
    data.metallic = GetFloat("", "metallic");
    data.alphaCutoff = GetFloat("", "alphaCutoff");
    data.baseColorTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.normalMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.metallicRoughnessTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
    data.emissiveMapTexIndex = glm::uvec2(Rendering::InvalidTextureIndex, 1);
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

    Rendering::GPUDrivenManager::Instance().UpdateMaterial(gpuMaterialHandle, data);
}
