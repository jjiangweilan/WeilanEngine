#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Rendering/GfxResourceTransfer.hpp"
namespace Engine
{
Material::Material() : shader(nullptr), shaderResource(nullptr)
{
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

Material::Material(RefPtr<Shader> shader) : Material() { SetShader(shader); }

Material::~Material(){};

void Material::SetTexture(const std::string& param, std::nullptr_t)
{
    textureValues.erase(param);
    shaderResource->SetTexture(param, nullptr);
}

void Material::SetTexture(const std::string& param, RefPtr<Texture> texture)
{
    textureValues[param] = texture;
    shaderResource->SetTexture(param, texture->GetGfxImage());
}

void Material::SetTexture(const std::string& param, RefPtr<Gfx::Image> image)
{
    shaderResource->SetTexture(param, image);
}

void Material::SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value)
{
    matrixValues[param + "." + member] = value;
    Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), param, member, (void*)&value);
}

void Material::SetFloat(const std::string& param, const std::string& member, float value)
{
    floatValues[param + "." + member] = value;
    Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), param, member, (void*)&value);
}

void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
{
    vectorValues[param + "." + member] = value;
    Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), param, member, (void*)&value);
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

RefPtr<Texture> Material::GetTexture(const std::string& param)
{
    auto iter = textureValues.find(param);
    if (iter != textureValues.end())
    {
        return iter->second;
    }
    return 0;
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
    if (shader != nullptr)
    {
        SetShaderNoProtection(shader);
    }
}

void Material::SetShaderNoProtection(RefPtr<Shader> shader)
{
    this->shader = shader;
    shaderConfig = shader->GetDefaultShaderConfig();
    shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(shader->GetShaderProgram(),
                                                                      Gfx::ShaderResourceFrequency::Material);
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

        Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), obj, mem, (void*)&v.second);
    }

    for (auto& v : matrixValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), obj, mem, (void*)&v.second);
    }

    for (auto& v : vectorValues)
    {
        auto dotIndex = v.first.find_first_of('.');
        auto obj = v.first.substr(0, dotIndex);
        auto mem = v.first.substr(dotIndex + 1);

        Internal::GetGfxResourceTransfer()->Transfer(shaderResource.Get(), obj, mem, (void*)&v.second);
    }

    // Note: When materials are deserialized from disk, OnReferenceResolve also works to set textures
    // That's why we check the existence of the "second"
    for (auto& v : textureValues)
    {
        if (v.second != nullptr)
            shaderResource->SetTexture(v.first, v.second->GetGfxImage());
    }
}

void Material::Serialize(Serializer* s)
{
    Resource::Serialize(s);
    s->Serialize("shader", shader);
    s->Serialize("floatValues", floatValues);
    s->Serialize("vectorValues", vectorValues);
    s->Serialize("matrixValues", matrixValues);
    s->Serialize("textureValues", textureValues);
}
void Material::Deserialize(Serializer* s)
{
    Resource::Deserialize(s);
    s->Deserialize("shader", shader, [this](void* res) { this->SetShader(this->shader); });
    s->Deserialize("floatValues", floatValues);
    s->Deserialize("vectorValues", vectorValues);
    s->Deserialize("matrixValues", matrixValues);
    s->Deserialize("textureValues",
                   textureValues,
                   [this](void* res)
                   {
                       if (res)
                       {
                           Texture* tex = (Texture*)res;
                           for (auto& kv : textureValues)
                           {
                               if (kv.second.Get() == tex)
                               {
                                   SetTexture(kv.first, tex);
                                   break;
                               }
                           }
                       }
                   });
}
} // namespace Engine
