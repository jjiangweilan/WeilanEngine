#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
namespace Engine
{
#define SER_MEMS() \
    SERIALIZE_MEMBER(shaderName);\
    SERIALIZE_MEMBER(floatValues);\
    SERIALIZE_MEMBER(vectorValues);\
    SERIALIZE_MEMBER(matrixValues);\
    SERIALIZE_MEMBER(textureValues);

    Material::Material() : shader(nullptr), shaderResource(nullptr)
    {
        SER_MEMS();
        assetReloadIterHandle = AssetDatabase::Instance()->RegisterOnAssetReload([this](RefPtr<AssetObject> obj) {
        Shader* casted = dynamic_cast<Shader*>(obj.Get());
        if (casted && this->shaderName == casted->GetName())
        {
            SetShaderNoProtection(this->shaderName);
            UpdateResources();
        }
        });
    }

    Material::Material(std::string_view shader):
        Material()
    {
        SER_MEMS();
        SetShader(shader);
    }

    Material::~Material()
    {
        AssetDatabase::Instance()->UnregisterOnAssetReload(assetReloadIterHandle);
    };

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
        // matrixValues[param] = value;
        shaderResource->SetUniform(param, member, (void*)&value);
    }

    void Material::SetFloat(const std::string& param, const std::string& member, float value)
    {
        floatValues[param + "." + member] = value;
        shaderResource->SetUniform(param, member, (void*)&value);
    }

    void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
    {
        vectorValues[param + "." + member] = value;
        shaderResource->SetUniform(param, member, (void*)&value);
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

    void Material::OnReferenceResolve(void* ptr, AssetObject* resolved)
    {
        Texture* casted = dynamic_cast<Texture*>(resolved);
        if (casted)
        {
            for(auto& tex : textureValues)
            {
                if (tex.second.Get() == resolved)
                {
                    SetTexture(tex.first, tex.second);
                    break;
                }
            }
        }
    }

    glm::vec4 Material::GetVector(const std::string &param, const std::string &member)
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

    void Material::SetShader(std::string_view shaderName)
    {
        if (this->shaderName != shaderName)
        {
            SetShaderNoProtection(shaderName);
        }
    }

    void Material::SetShaderNoProtection(std::string_view shaderName)
    {
        auto newShader = AssetDatabase::Instance()->GetShader(std::string(shaderName));

        if (newShader)
        {
            this->shader = newShader;
            this->shaderName = shaderName;
            shaderConfig = shader->GetDefaultShaderConfig();
            shaderResource = Gfx::GfxDriver::Instance()->CreateShaderResource(shader->GetShaderProgram(), Gfx::ShaderResourceFrequency::Material);
            UpdateResources();
        }
    }

    void Material::DeserializeInternal(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver)
    {
        AssetObject::DeserializeInternal(nameChain, serializer, refResolver);

        if (!shaderName.empty())
        {
            SetShaderNoProtection(shaderName);
        }
    }

    void Material::UpdateResources()
    {
        if (shaderResource == nullptr) return;
        for(auto& v : floatValues)
        {
            shaderResource->SetUniform(v.first, &v.second);
        }

        for(auto& v : matrixValues)
        {
            shaderResource->SetUniform(v.first, &v.second);
        }

        for(auto& v : vectorValues)
        {
            shaderResource->SetUniform(v.first, &v.second);
        }

        // Note: When materials are deserialized from disk, OnReferenceResolve also works to set textures
        // That's why we check the existence of the "second"
        for (auto& v : textureValues)
        {
            if (v.second != nullptr)
                shaderResource->SetTexture(v.first, v.second->GetGfxImage());
        }
    }
}
