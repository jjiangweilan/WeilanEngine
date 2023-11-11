#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/ImmediateGfx.hpp"
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

Material::Material(const Material& other)
    : Asset(other), shader(nullptr), shaderConfig(other.shaderConfig), floatValues(other.floatValues),
      vectorValues(other.vectorValues), matrixValues(other.matrixValues), textureValues(other.textureValues)
{
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

void TransferToGPU(
    Gfx::ShaderResource* shaderResource, const std::string& param, const std::string& member, uint8_t* data
)
{
    Gfx::ShaderResource::BufferMemberInfoMap memberInfo;
    auto dstBuffer = shaderResource->GetBuffer(param, memberInfo);
    auto memberInfoIter = memberInfo.find(member);
    if (memberInfoIter == memberInfo.end())
        return;

    auto byteSize = memberInfoIter->second.size;

    Gfx::Buffer::CreateInfo bufCreateInfo;
    bufCreateInfo.size = byteSize;
    bufCreateInfo.usages = Gfx::BufferUsage::Transfer_Src;
    bufCreateInfo.debugName = "material transfer buffer";
    bufCreateInfo.visibleInCPU = true;
    auto stagingBuffer = Gfx::GfxDriver::Instance()->CreateBuffer(bufCreateInfo);
    memcpy(stagingBuffer->GetCPUVisibleAddress(), data, byteSize);
    auto bufOffset = memberInfoIter->second.offset;

    ImmediateGfx::OnetimeSubmit(
        [bufOffset, byteSize, dstBuffer, &stagingBuffer](Gfx::CommandBuffer& cmd)
        {
            Gfx::BufferCopyRegion bufferCopyRegions[] = {{0, bufOffset, byteSize}};
            cmd.CopyBuffer(stagingBuffer, dstBuffer, bufferCopyRegions);
        }
    );
}

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    matrixValues[param + "." + member] = value;
    if (shaderResource != nullptr)
        TransferToGPU(shaderResource.Get(), param, member, (uint8_t*)&value);
    SetDirty();
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    floatValues[param + "." + member] = value;
    if (shaderResource != nullptr)
        TransferToGPU(shaderResource.Get(), param, member, (uint8_t*)&value);
    SetDirty();
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    vectorValues[param + "." + member] = value;
    if (shaderResource != nullptr)
        TransferToGPU(shaderResource.Get(), param, member, (uint8_t*)&value);
    SetDirty();
}

glm::mat4 Material::GetMatrix(const std::string& param, const std::string& member)
{
    auto iter = matrixValues.find(param + "." + member);
    if (iter != matrixValues.end())
    {
        return iter->second;
    }
    return glm::mat4(0);
}

glm::vec4 Material::GetVector(const std::string& param, const std::string& member)
{
    auto iter = vectorValues.find(param + "." + member);
    if (iter != vectorValues.end())
    {
        return iter->second;
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
    auto iter = floatValues.find(param + "." + member);
    if (iter != floatValues.end())
    {
        return iter->second;
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
    if (shaderResource != nullptr)
    {
        GetGfxDriver()->WaitForIdle();
    }
    shaderResource =
        Gfx::GfxDriver::Instance()->CreateShaderResource(GetShaderProgram(), Gfx::ShaderResourceFrequency::Material);
    UpdateResources();
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

        TransferToGPU(shaderResource.Get(), obj, mem, (uint8_t*)&v.second);
    }

    for (auto& v : matrixValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        TransferToGPU(shaderResource.Get(), obj, mem, (uint8_t*)&v.second);
    }

    for (auto& v : vectorValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        TransferToGPU(shaderResource.Get(), obj, mem, (uint8_t*)&v.second);
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
    s->Serialize("floatValues", floatValues);
    s->Serialize("vectorValues", vectorValues);
    s->Serialize("matrixValues", matrixValues);
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
    if (GetShaderProgram() != shaderResource->GetShaderProgram())
    {
        SetShaderNoProtection(shader);
    }

    return shaderResource.Get();
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
        cachedShaderProgram = shader->GetShaderProgram(features);
    }

    return cachedShaderProgram;
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
    s->Deserialize("floatValues", floatValues);
    s->Deserialize("vectorValues", vectorValues);
    s->Deserialize("matrixValues", matrixValues);
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
} // namespace Engine
