#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/ImmediateGfx.hpp"
#include "Rendering/RenderPipeline.hpp"
namespace Engine
{
DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");

Material::Material() : shader(nullptr), shaderResource(nullptr)
{
    SetName("new material");
    // assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload(
    //     [this](RefPtr<AssetObject> obj)
    //     {
    //         Shader* casted = dynamic_cast<Shader*>(obj.Get());
    //         if (casted && this->shaderName == casted->GetName())
    //         {
    //             SetShaderNoProtection(this->shaderName);
    //             UpdateResources();
    //         }
    //     });
}

Material::Material(const Material& other) : Asset(other), shader(nullptr), shaderConfig(other.shaderConfig)
{
    for (auto& ubo : other.ubos)
    {
        auto& thisUbo = this->ubos[ubo.first];
        thisUbo.buffer = nullptr;
        thisUbo.floats = ubo.second.floats;
        thisUbo.vectors = ubo.second.vectors;
        thisUbo.matrices = ubo.second.matrices;
    }

    if (other.shader)
        SetShader(other.shader);
}

Material::Material(RefPtr<Shader> shader) : Material()
{
    SetName("new material");
    SetShader(shader);
}

Material::~Material(){};

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    if (shaderResource != nullptr)
        shaderResource->SetImage(param, nullptr);
    SetDirty();
}

void Material::SetTexture(const std::string& param, Texture* texture)
{
    textureValues[param] = texture;
    if (shaderResource != nullptr)
        shaderResource->SetImage(param, texture->GetGfxImage());
    SetDirty();
}

void Material::SetTexture(const std::string& param, Gfx::Image* image)
{
    if (shaderResource != nullptr)
        shaderResource->SetImage(param, image);
    SetDirty();
}

void Material::TransferToGPU(
    Gfx::ShaderResource* shaderResource, const std::string& param, const std::string& member, uint8_t* data
)
{
    auto dstBuffer = shaderResource->GetBuffer(param);
    if (dstBuffer == nullptr)
        return;

    auto shaderProgram = GetShaderProgram();
    auto& bindings = shaderProgram->GetShaderInfo().bindings;
    auto iter = bindings.find(param);
    if (iter == bindings.end())
    {
        return;
    }

    auto& binding = iter->second;
    uint32_t offset = 0;
    uint32_t size = 0;
    if (binding.type == Gfx::ShaderInfo::BindingType::UBO)
    {
        auto& mems = binding.binding.ubo.data.members;
        auto memIter = mems.find(member);
        if (memIter == mems.end())
        {
            offset = memIter->second.offset;
            size = memIter->second.data->size;
        }
    }
    else if (binding.type == Gfx::ShaderInfo::BindingType::SSBO)
    {
        auto& mems = binding.binding.ssbo.data.members;
        auto memIter = mems.find(member);
        if (memIter == mems.end())
        {
            offset = memIter->second.offset;
            size = memIter->second.data->size;
        }
    }
    else
        return;

    if (size == 0)
        return;

    RenderPipeline::Singleton().UploadBuffer(*dstBuffer, data, size, offset);
}

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    ubos[param].matrices[member] = value;
    SetDirty();
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    ubos[param].floats[member] = value;
    SetDirty();
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    ubos[param].vectors[member] = value;
    SetDirty();
}

glm::mat4 Material::GetMatrix(const std::string& param, const std::string& member)
{
    auto iter = ubos.find(param);
    if (iter != ubos.end())
    {
        auto memIter = iter->second.matrices.find(member);
        if (memIter != iter->second.matrices.end())
        {
            return memIter->second;
        }
    }
    return glm::mat4(0);
}

glm::vec4 Material::GetVector(const std::string& param, const std::string& member)
{
    auto iter = ubos.find(param);
    if (iter != ubos.end())
    {
        auto memIter = iter->second.vectors.find(member);
        if (memIter != iter->second.vectors.end())
        {
            return memIter->second;
        }
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
    auto iter = ubos.find(param);
    if (iter != ubos.end())
    {
        auto memIter = iter->second.floats.find(member);
        if (memIter != iter->second.floats.end())
        {
            return memIter->second;
        }
    }
    return 0;
}

void Material::SetShader(RefPtr<Shader> shader)
{
    if (shader != nullptr) // why null check? I think we don't need it
    {
        SetShaderNoProtection(shader);

        SetDirty();
    }
}

void Material::SetShaderNoProtection(RefPtr<Shader> shader)
{
    this->shader = shader.Get();
    shaderConfig = shader->GetDefaultShaderConfig();
    // if (shaderResource != nullptr)
    // {
    //     GetGfxDriver()->WaitForIdle();
    // }
    shaderResource = GetGfxDriver()->CreateShaderResource();
}

void Material::UpdateResources()
{
    if (shaderResource == nullptr)
        return;

    for (auto& v : floatValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        TransferToGPU(shaderResource.get(), obj, mem, (uint8_t*)&v.second);
    }

    for (auto& v : matrixValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        TransferToGPU(shaderResource.get(), obj, mem, (uint8_t*)&v.second);
    }

    for (auto& v : vectorValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        TransferToGPU(shaderResource.get(), obj, mem, (uint8_t*)&v.second);
    }

    // Note: When materials are deserialized from disk, OnReferenceResolve also works to set textures
    // That's why we check the existence of the "second"
    for (auto& v : textureValues)
    {
        if (v.second != nullptr)
            shaderResource->SetImage(v.first, v.second->GetGfxImage());
    }
}

void Material::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shader", shader);
    s->Serialize("ubos", ubos);
    s->Serialize("textureValues", textureValues);
    std::vector<std::string> enabledFeatureVec(enabledFeatures.begin(), enabledFeatures.end());
    s->Serialize("enabledFeature", enabledFeatureVec);
}

std::unique_ptr<Asset> Material::Clone()
{
    return std::unique_ptr<Material>(new Material(*this));
}

Gfx::ShaderResource* Material::ValidateGetShaderResource()
{
    // if (GetShaderProgram() != shaderResource->GetShaderProgram())
    // {
    //     SetShaderNoProtection(shader);
    // }

    return shaderResource.get();
}

Gfx::ShaderProgram* Material::GetShaderProgram()
{
    uint64_t globalShaderFeaturesHash = Shader::GetEnabledFeaturesHash();

    if (cachedShaderProgram == nullptr || this->globalShaderFeaturesHash != globalShaderFeaturesHash)
    {
        this->globalShaderFeaturesHash = globalShaderFeaturesHash;
        std::vector<std::string> features(enabledFeatures.begin(), enabledFeatures.end());
        auto& globalEnabledFeatures = Shader::GetEnabledFeatures();
        features.insert(features.end(), globalEnabledFeatures.begin(), globalEnabledFeatures.end());
        auto newProgram = shader->GetShaderProgram(features);

        if (newProgram != cachedShaderProgram)
        {
            cachedShaderProgram = newProgram;
            CreateBuffer();
        }
    }

    return cachedShaderProgram;
}

void Material::CreateBuffer()
{
    for (auto& b : cachedShaderProgram->GetShaderInfo().bindings)
    {
        if (b.second.type == Gfx::ShaderInfo::BindingType::UBO)
        {
            size_t size = b.second.binding.ubo.data.size;
            std::unique_ptr<Gfx::Buffer> buffer = GetGfxDriver()->CreateBuffer({
                .usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
                .size = size,
                .visibleInCPU = false,
                .debugName = "Material Uniform Buffer",
            });

            shaderResource->SetBuffer(b.first, buffer.get());
            ubos[b.first].buffer = std::move(buffer);

            UpdatePendingBuffer();
        }
    }
}

void Material::EnableFeature(const std::string& name)
{
    if (!enabledFeatures.contains(name))
    {
        enabledFeatures.emplace(name);
        cachedShaderProgram = nullptr;
    }
}

void Material::DisableFeature(const std::string& name)
{
    if (enabledFeatures.contains(name))
    {
        enabledFeatures.erase(name);
        cachedShaderProgram = nullptr;
    }
}

void Material::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("shader", shader, [this](void* res) { this->SetShaderNoProtection(this->shader); });
    s->Deserialize("ubos", ubos);
    s->Deserialize(
        "textureValues",
        textureValues,
        [this](void* res)
        {
            if (res)
            {
                Texture* tex = (Texture*)res;
                for (auto& kv : textureValues)
                {
                    if (kv.second == tex)
                    {
                        SetTexture(kv.first, tex);
                        break;
                    }
                }
            }
        }
    );
    std::vector<std::string> enabledFeatureVec;
    s->Deserialize("enabledFeature", enabledFeatureVec);
    for (auto& f : enabledFeatureVec)
    {
        EnableFeature(f);
    }
}

void Material::UBO::Serialize(Serializer* ser)
{
    ser->Serialize("floats", floats);
    ser->Serialize("vectors", vectors);
    ser->Serialize("matrices", matrices);
}

void Material::UBO::Deserialize(Serializer* ser)
{
    ser->Deserialize("floats", floats);
    ser->Deserialize("vector", vectors);
    ser->Deserialize("matrices", matrices);
}

void Material::UpdateBuffer(const std::string& bindingName)
{
    auto iter = ubos.find(bindingName);
    if (iter != ubos.end())
    {
        if (iter->second.buffer == nullptr)
        {
            // pending to update
        }
        else
        {
            // schedule update
        }
    }
}

void Material::UpdatePendingBuffer() {}
} // namespace Engine
