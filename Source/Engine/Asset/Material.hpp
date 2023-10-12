#pragma once

#include "Core/Asset.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Shader.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
namespace Engine
{
namespace Gfx
{
class ShaderResource;
}

class AssetDatabase;
class AssetFileData;
class Material : public Asset
{
    DECLARE_ASSET();

public:
    Material(RefPtr<Shader> shader);
    Material();
    Material(const Material& other);
    ~Material() override;

    void SetShader(RefPtr<Shader> shader);
    Shader* GetShader()
    {
        return shader;
    }

    RefPtr<Gfx::ShaderResource> GetShaderResource()
    {
        return shaderResource;
    }

    std::unique_ptr<Asset> Clone() override;

    void SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value);
    void SetFloat(const std::string& param, const std::string& member, float value);
    void SetVector(const std::string& param, const std::string& member, const glm::vec4& value);
    void SetTexture(const std::string& param, Gfx::Image* image);
    void SetTexture(const std::string& param, Texture* texture);
    void SetTexture(const std::string& param, std::nullptr_t);

    glm::mat4 GetMatrix(const std::string& param, const std::string& membr);
    Texture* GetTexture(const std::string& param);
    float GetFloat(const std::string& param, const std::string& membr);
    glm::vec4 GetVector(const std::string& param, const std::string& membr);

    // const UUID& Serialize(RefPtr<AssetFileData> assetFileData) override;
    // void        Deserialize(RefPtr<AssetFileData> assetFileData, RefPtr<AssetDatabase> assetDatabase) override;

    const Gfx::ShaderConfig& GetShaderConfig()
    {
        if (overrideShaderConfig || shader == nullptr)
            return shaderConfig;
        else
            return shader->GetDefaultShaderConfig();
    }

    void SetShaderConfig(const Gfx::ShaderConfig& shaderConfig)
    {
        overrideShaderConfig = true;
        this->shaderConfig = shaderConfig;
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    Shader* shader = nullptr;
    UniPtr<Gfx::ShaderResource> shaderResource = nullptr;
    Gfx::ShaderConfig shaderConfig;

    bool overrideShaderConfig = false;
    std::unordered_map<std::string, float> floatValues;
    std::unordered_map<std::string, glm::vec4> vectorValues;
    std::unordered_map<std::string, glm::mat4> matrixValues;
    std::unordered_map<std::string, Texture*> textureValues;

    void UpdateResources();
    void SetShaderNoProtection(RefPtr<Shader> shader);
    static int initImporter_;
};
} // namespace Engine
