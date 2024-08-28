#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
DEFINE_ASSET(Material, "9D87873F-E8CB-45BB-AD28-225B95ECD941", "mat");

Material::Material() : shader(nullptr), shaderResource(nullptr)
{
    SetName("new material");
    schedule = std::make_shared<Schedule>();
    shaderResource = GetGfxDriver()->CreateShaderResource();
}

// Material::Material(const Material& other) : Asset(other), shader(nullptr), shaderConfig(other.shaderConfig)
//{
//     for (auto& ubo : other.ubos)
//     {
//         auto& thisUbo = this->ubos[ubo.first];
//         thisUbo.buffer = nullptr;
//         thisUbo.floats = ubo.second.floats;
//         thisUbo.vectors = ubo.second.vectors;
//         thisUbo.matrices = ubo.second.matrices;
//     }
//
//     schedule = std::make_shared<Schedule>();
//
//     if (other.shader)
//         SetShader(other.shader);
//
//     shaderResource = GetGfxDriver()->CreateShaderResource();
// }

Material::Material(ShaderBase* shader) : Material()
{
    SetName("new material");
    SetShader(shader);

    schedule = std::make_shared<Schedule>();
    shaderResource = GetGfxDriver()->CreateShaderResource();
}

Material::~Material(){};

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    if (shaderResource != nullptr)
        shaderResource->Remove(param);
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

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    auto& u = ubos[param];
    u.matrices[member] = value;
    u.dirty = true;
    uploadNeeded = true;
    SetDirty();
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    auto& u = ubos[param];
    u.floats[member] = value;
    u.dirty = true;
    uploadNeeded = true;
    SetDirty();
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    auto& u = ubos[param];
    u.vectors[member] = value;
    u.dirty = true;
    uploadNeeded = true;
    SetDirty();
}

glm::mat4 Material::GetMatrix(const std::string& param, const std::string& member)
{
    uploadNeeded = true;
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
    uploadNeeded = true;
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
    uploadNeeded = true;
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

void Material::SetShader(ShaderBase* shader)
{
    if (shader != nullptr) // why null check? I think we don't need it
    {
        SetShaderNoProtection(shader);
        SetDirty();
    }
}

void Material::SetShaderNoProtection(ShaderBase* shader)
{
    this->shader = shader;
    uploadNeeded = true;
    shaderConfig = shader->GetDefaultShaderConfig();
    // if (shaderResource != nullptr)
    // {
    //     GetGfxDriver()->WaitForIdle();
    // }
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
    return nullptr;
    // return std::unique_ptr<Material>(new Material(*this));
}

Gfx::ShaderResource* Material::ValidateGetShaderResource()
{
    return shaderResource.get();
}

Gfx::ShaderProgram* Material::GetShaderProgram(int shaderPassIndex)
{
    if (!shader)
        return nullptr;

    uint64_t globalShaderFeaturesHash = Shader::GetEnabledFeaturesHash();
    if (cachedShaderPrograms[shaderPassIndex] == nullptr ||
        this->globalShaderFeaturesHash != globalShaderFeaturesHash || shaderContentHash != shader->GetContentHash())
    {
        shaderContentHash = shader->GetContentHash();

        this->globalShaderFeaturesHash = globalShaderFeaturesHash;
        cachedShaderProgramFeatures = std::vector<std::string>(enabledFeatures.begin(), enabledFeatures.end());
        auto& globalEnabledFeatures = Shader::GetEnabledFeatures();
        cachedShaderProgramFeatures
            .insert(cachedShaderProgramFeatures.end(), globalEnabledFeatures.begin(), globalEnabledFeatures.end());
        auto newProgram = shader->GetShaderProgram(
            shaderPassIndex,
            shader->GetShaderFeatureBitmask(shaderPassIndex, cachedShaderProgramFeatures)
        );

        if (newProgram != cachedShaderPrograms[shaderPassIndex])
        {
            cachedShaderPrograms[shaderPassIndex] = newProgram;
            for (auto& u : ubos)
            {
                u.second.buffer = nullptr; // recreation needed
                u.second.dirty = true;
                uploadNeeded = true;
            }
        }
    }

    Gfx::ShaderProgram* shaderProgram = cachedShaderPrograms[shaderPassIndex];

    if (uploadNeeded)
    {
        UploadDataToGPU(shaderProgram);
    }

    return shaderProgram;
}

void Material::EnableFeature(const std::string& name)
{
    if (!enabledFeatures.contains(name))
    {
        enabledFeatures.emplace(name);
        cachedShaderPrograms.clear();
    }
}

void Material::DisableFeature(const std::string& name)
{
    if (enabledFeatures.contains(name))
    {
        enabledFeatures.erase(name);
        cachedShaderPrograms.clear();
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
    uploadNeeded = true;
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

    uploadNeeded = false;

    for (auto& u : ubos)
    {
        if (u.second.dirty)
        {
            u.second.dirty = false;
            auto bindingIter = shaderProgram->GetShaderInfo().bindings.find(u.first);
            if (bindingIter != shaderProgram->GetShaderInfo().bindings.end() &&
                bindingIter->second.type == Gfx::ShaderInfo::BindingType::UBO)
            {
                // Create the buffer
                if (u.second.buffer == nullptr)
                {
                    size_t size = bindingIter->second.binding.ubo.data.size;
                    std::unique_ptr<Gfx::Buffer> buffer = GetGfxDriver()->CreateBuffer({
                        .usages = Gfx::BufferUsage::Transfer_Dst | Gfx::BufferUsage::Uniform,
                        .size = size,
                        .visibleInCPU = false,
                        .debugName = "Material Uniform Buffer",
                    });

                    shaderResource->SetBuffer(u.first, buffer.get());
                    u.second.buffer = std::move(buffer);
                }

                auto bufSize = bindingIter->second.binding.ubo.data.size;
                tempUploadData.clear();
                tempUploadData.resize(bufSize);

                auto& members = bindingIter->second.binding.ubo.data.members;
                for (auto& m : members)
                {
                    switch (m.second.data->type)
                    {
                        case Gfx::ShaderInfo::ShaderDataType::Float:
                            {
                                auto iter = u.second.floats.find(m.first);
                                if (iter != u.second.floats.end())
                                {
                                    size_t offset = m.second.offset;
                                    assert(offset + sizeof(float) <= bufSize);
                                    *((float*)(tempUploadData.data() + offset)) = iter->second;
                                }
                                break;
                            }
                        case Gfx::ShaderInfo::ShaderDataType::UInt:
                            {
                                auto iter = u.second.floats.find(m.first);
                                if (iter != u.second.floats.end())
                                {
                                    size_t offset = m.second.offset;
                                    assert(offset + sizeof(float) <= bufSize);
                                    *((uint32_t*)(tempUploadData.data() + offset)) = (uint32_t)iter->second;
                                }
                                break;
                            }
                        case Gfx::ShaderInfo::ShaderDataType::Vec4:
                        case Gfx::ShaderInfo::ShaderDataType::Vec3:
                            {
                                auto iter = u.second.vectors.find(m.first);
                                if (iter != u.second.vectors.end())
                                {
                                    size_t offset = m.second.offset;
                                    assert(offset + sizeof(glm::vec4) <= bufSize);
                                    *((glm::vec4*)(tempUploadData.data() + offset)) = iter->second;
                                }
                                break;
                            }
                        case Gfx::ShaderInfo::ShaderDataType::Vec2:
                            {
                                auto iter = u.second.vectors.find(m.first);
                                if (iter != u.second.vectors.end())
                                {
                                    size_t offset = m.second.offset;
                                    assert(offset + sizeof(glm::vec2) <= bufSize);
                                    *((glm::vec2*)(tempUploadData.data() + offset)) = glm::vec2(iter->second);
                                }
                                break;
                            }
                        case Gfx::ShaderInfo::ShaderDataType::Mat4:
                            {
                                auto iter = u.second.matrices.find(m.first);
                                if (iter != u.second.matrices.end())
                                {
                                    size_t offset = m.second.offset;
                                    assert(offset + sizeof(glm::mat4) <= bufSize);
                                    *((glm::mat4*)(tempUploadData.data() + offset)) = iter->second;
                                }
                                break;
                            }
                    }
                }
                GetGfxDriver()->UploadBuffer(*u.second.buffer, tempUploadData.data(), tempUploadData.size(), 0);
            }
        }
    }
}
