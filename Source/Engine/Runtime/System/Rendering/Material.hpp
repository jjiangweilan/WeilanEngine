#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Driver/GfxDriver/Buffer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/PipelineConfig.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/Rendering/GPUDriven/GPUDrivenManager.hpp"
#include "Engine/Runtime/System/Rendering/Shader.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <atomic>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    Material(ObjPtr<Shader> shader);
    Material(const Material& src) : Material() { Copy(src); }
    Material& operator=(const Material& src)
    {
        Copy(src);
        return *this;
    };
    ~Material() override;

    void Copy(const Material& src);

    void SetName(std::string_view name) override
    {
        Asset::SetName(name);
        if (shaderResource)
            shaderResource->SetName(name);
    }

    void SetShader_Lua(const char* shaderName) { SetShader(shaderName); }
    void SetShader(std::string_view shaderName);
    ObjPtr<Shader> GetShader() { return shaderInUse; }
    void SetShader(Shader* shader);
    void SetShader(Shaders shader);

    void OverrideDescriptorSet(Gfx::DescriptorSetSemantics set) { targetDescriptorSet = set; }

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
    void SetTextureSamplerIndex(const std::string& bindingName, uint32_t samplerIndex);
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

    const Gfx::PipelineConfig& GetPipelineConfig();

    void SetPipelineConfig(const Gfx::PipelineConfig::PipelineConfig_t& pipelineConfig)
    {
        overridePipelineConfig = true;
        this->pipelineConfig = pipelineConfig;
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

    // GPU-Driven bindless support
    uint32_t GetGpuMaterialOffset() { return Rendering::GPUDrivenManager::Instance().GetMaterialDescriptor(gpuMaterialHandle).dataAlloc.offset; }
    Rendering::GPUMaterialHandle GetGPUMaterialHandle() const { return gpuMaterialHandle; }
    bool IsGPUMaterialRegistered() const { return gpuMaterialHandle != Rendering::InvalidGPUHandle; }
    bool HasPendingGPUMaterialUpload() const { return gpuMaterialUploadNeeded; }
    void RegisterGPUMaterial();
    void UnregisterGPUMaterial();
    void UpdateGPUMaterialData();
    void SetGPUDrivenExtraData(std::span<const uint8_t> data);

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void SetGPUDrivenExtraData(const T& data)
    {
        SetGPUDrivenExtraData(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&data),
            sizeof(T)
        ));
    }

private:
    struct UBO
    {
        UBO& operator=(const UBO& src)
        {
            dirty = true;
            floats = src.floats;
            vectors = src.vectors;
            matrices = src.matrices;
            buffer = nullptr; // created upon first use

            return *this;
        };

        bool dirty = false;
        std::unique_ptr<Gfx::Buffer> buffer;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, glm::vec4> vectors;
        std::unordered_map<std::string, glm::mat4> matrices;

        void Serialize(Serializer* ser) const;
        void Deserialize(Serializer* ser);
        void CreateUBO();
    };

    UBO ubo;
    std::string shaderName;
    const ShaderFeatures* shaderFeatures = nullptr;
    ObjPtr<Shader> shaderInUse = nullptr;
    Gfx::PipelineConfig pipelineConfig;
    bool overridePipelineConfig = false;
    Gfx::DescriptorSetSemantics targetDescriptorSet = Gfx::DescriptorSetSemantics::Material;

    // std::unordered_map<std::string, UBO> ubos;
    std::unordered_map<std::string, ObjPtr<Texture>> textureValues;
    std::unordered_map<std::string, uint32_t> textureSamplerIndices;
    std::unordered_map<std::string, std::optional<Gfx::ImageViewOption>> textureImageViewOptions;
    std::unordered_map<std::string, Gfx::Buffer*> bufferValues;
    std::unordered_set<std::string> enabledFeatures;

    // ============ Runtime =============/
    bool uploadNeeded = false;
    bool needRequestNewShader = false;
    std::unique_ptr<Gfx::ShaderResource> shaderResource = nullptr;

    // GPU-Driven bindless
    Rendering::GPUMaterialHandle gpuMaterialHandle = Rendering::InvalidGPUHandle;
    std::atomic_bool gpuMaterialUploadNeeded = false;
    std::vector<uint8_t> gpuExtraData;

    void UploadDataToGPU(Gfx::ShaderProgram* shaderProgram);
    void WriteParameterDataToBuffer(
        const Gfx::ShaderPipelineInfo& pipeline,
        const Gfx::ShaderPipelineInfo::BufferMember& bufferDataDescription,
        uint8_t* buf,
        size_t bufSize
    );
    void SetShaderNoProtection(ObjPtr<Shader> shaderProgram);
    Gfx::ShaderResource* ValidateGetShaderResource();
    void SetTextureInternal(
        const std::string& param, Texture* texture, std::optional<Gfx::ImageViewOption> imageViewOption
    );
    Rendering::GpuMaterial BuildGPUMaterialData() const;
    void InitializeSceneLitDefaults();
    void InitializeTreeWindDefaults();
    void MarkGPUMaterialUploadNeeded();
};
