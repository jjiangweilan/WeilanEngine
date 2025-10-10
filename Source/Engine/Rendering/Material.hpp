#pragma once

#include "Core/Asset.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/Buffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/ShaderConfig.hpp"
#include "Rendering/Shader2.hpp"
#include "Rendering/ShaderLibrary.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

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
    Material();
    Material(std::string_view shaderName);
    Material(ObjPtr<Shader2> shader);
    Material(const Material& other) = delete;
    ~Material() override;

    void SetName(std::string_view name) override
    {
        Asset::SetName(name);
        if (shaderResource)
            shaderResource->SetName(name);
    }

    void SetShader_Lua(const char* shaderName) { SetShader(shaderName); }
    void SetShader(std::string_view shaderName);
    ObjPtr<Shader2> GetShader() { return shaderInUse; }
    void SetShader(Shader2* shader);
    void SetShader(Shaders shader);

    Gfx::ShaderProgram* GetShaderProgram();

    Gfx::ShaderResource* GetShaderResource() { return ValidateGetShaderResource(); }

    std::unique_ptr<Asset> Clone() override;

    void SetBuffer(const std::string& name, Gfx::Buffer* buffer);
    void SetMatrix(const std::string& name, const glm::mat4& value);
    void SetFloat(const std::string& name, float value);
    void SetVector(const std::string& name, const glm::vec4& value);
    void SetMatrix(const std::string& param, const std::string& member, const glm::mat4& value);
    void SetFloat(const std::string& param, const std::string& member, float value);
    void SetVector(const std::string& param, const std::string& member, const glm::vec4& value);
    int GetSet(const std::string& name) const;
    int GetSet(Gfx::DescriptorSetSemantics semantics) const;

    void SetTexture_Lua(const std::string& param, const ObjPtr<Texture>& texture) { return SetTexture(param, texture, std::nullopt); }


    void RawSetTexture(const std::string& param, const ObjPtr<Texture>& texture);
    void SetTexture(const std::string& param, Gfx::Image* image, std::optional<Gfx::ImageViewOption> imageViewOption = std::nullopt);
    void SetTexture(
        const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption = std::nullopt
    );
    void SetTexture(const std::string& param, std::nullptr_t);
    void EnableFeature(const std::string& name);
    void DisableFeature(const std::string& name);

    glm::mat4 GetMatrix(const std::string& param, const std::string& membr);
    Texture* GetTexture(const std::string& param);
    float GetFloat(const std::string& param, const std::string& membr);
    glm::vec4 GetVector(const std::string& param, const std::string& membr);

    // const UUID& Serialize(RefPtr<AssetFileData> assetFileData) override;
    // void        Deserialize(RefPtr<AssetFileData> assetFileData, RefPtr<AssetDatabase> assetDatabase) override;

    const Gfx::PipelineConfig& GetShaderConfig();

    void SetShaderConfig(const Gfx::PipelineConfig::PipelineConfig_t& shaderConfig)
    {
        overrideShaderConfig = true;
        this->shaderConfig = shaderConfig;
    }

    void OnLoaded() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    void CopyProperties(Material& other);

    std::vector<std::string> GetCachedShaderProgramFeatureUsed() const { return {}; }

    const std::unordered_set<std::string>& GetEnabledFeatures() const { return enabledFeatures; }
    bool IsFeatureEnabled(const std::string& feature) const
    {
        return enabledFeatures.find(feature) != enabledFeatures.end();
    }

    // a dirty implementation to use when a texture is reimported in editor
    static void RebuildAllMaterials();

private:
    struct UBO
    {
        bool dirty = false;
        std::unique_ptr<Gfx::Buffer> buffer;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, glm::vec4> vectors;
        std::unordered_map<std::string, glm::mat4> matrices;

        void Serialize(Serializer* ser) const;
        void Deserialize(Serializer* ser);
    } ubo;

    std::string shaderName;
    const ShaderFeatures* shaderFeatures = nullptr;
    ObjPtr<Shader2> shaderInUse = nullptr;
    std::unique_ptr<Gfx::ShaderResource> shaderResource = nullptr;
    Gfx::PipelineConfig shaderConfig;
    bool overrideShaderConfig = false;

    // std::unordered_map<std::string, UBO> ubos;
    std::unordered_map<std::string, ObjPtr<Texture>> textureValues;
    std::unordered_map<std::string, std::optional<Gfx::ImageViewOption>> textureImageViewOptions;
    std::unordered_map<std::string, Gfx::Buffer*> bufferValues;
    std::unordered_set<std::string> enabledFeatures;
    bool uploadNeeded = false;
    bool needRequestNewShader = false;

    void UploadDataToGPU(Gfx::ShaderProgram* shaderProgram);
    void WriteParameterDataToBuffer(
        const Gfx::PipelineInfo& pipeline,
        const Gfx::PipelineInfo::BufferMember& bufferDataDescription,
        uint8_t* buf,
        size_t bufSize
    );
    void SetShaderNoProtection(ObjPtr<Shader2> shaderProgram);
    Gfx::ShaderResource* ValidateGetShaderResource();
    void SetTextureInternal(
        const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
    );
};
