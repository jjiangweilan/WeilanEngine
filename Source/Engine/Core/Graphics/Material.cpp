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
        //floatValues[param] = value;
        shaderResource->SetUniform(param, member, (void*)&value);
    }

    void Material::SetVector(const std::string& param, const std::string& member, const glm::vec4& value)
    {
        shaderResource->SetUniform(param, member, (void*)&value);
    }

    const glm::mat4& Material::GetMatrix(const std::string& param)
    {
        return matrixValues[param];
    }

    float Material::GetFloat(const std::string& param)
    {
        return floatValues[param];
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
        shader = AssetDatabase::Instance()->GetShader(std::string(shaderName));

        if (shader)
        {
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
        for(auto& v : floatValues)
        {
            shaderResource->SetUniform(v.first, &v.second);
        }

        for(auto& v : matrixValues)
        {
            shaderResource->SetUniform(v.first, &v.second);
        }
    }
}
