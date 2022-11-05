#include "Material.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
namespace Engine
{
    Material::Material() : shaderResource(nullptr)
    {}

    Material::Material(std::string_view shader):
        shader(nullptr),
        shaderResource(nullptr)
    {
        SetShader(shader);
    }

    Material::~Material()
    {
    };

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
        }
    }

    void Material::Deserialize(AssetSerializer& serializer, ReferenceResolver& refResolver)
    {
        AssetObject::Deserialize(serializer, refResolver);

        if (!shaderName.empty())
        {
            SetShaderNoProtection(shaderName);
            UpdateResources();
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

        // todo texture reference needs to be resolved
    }
}
