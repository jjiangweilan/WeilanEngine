#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Libs/Ptr.hpp"
#include <set>
#include <string>
namespace Gfx
{
class ShaderProgram;
class ShaderLoader;
} // namespace Gfx

class ShaderBase : public Asset
{
public:
    ShaderBase(){};
    ShaderBase(
        const std::string& name,
        std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram,
        const UUID& uuid = UUID::GetEmptyUUID()
    );
    void Reload(Asset&& loaded) override;
    ~ShaderBase() override {}

    // get a shader program with enabled global shader features
    Gfx::ShaderProgram* GetDefaultShaderProgram();

    Gfx::ShaderProgram* GetShaderProgram(const std::vector<std::string>& enabledFeature)
    {
        uint64_t id = GetFeaturesID(enabledFeature);

        return GetShaderProgram(0, id);
    }
    Gfx::ShaderProgram* GetShaderProgram(std::string_view shaderPass, const std::vector<std::string>& enabledFeature);
    Gfx::ShaderProgram* GetShaderProgram(size_t enabledFeatureHash);
    Gfx::ShaderProgram* GetShaderProgram(int shaderPass, size_t enabledFeatureHash);
    int GetPassCount() const
    {
        return shaderPasses.size();
    }

    bool NeedReimport() override;

    int FindShaderPass(std::string_view name);

    inline const Gfx::ShaderConfig& GetDefaultShaderConfig()
    {
        return shaderPasses[0]->shaderPrograms[0]->GetDefaultShaderConfig();
    }

    bool IsExternalAsset() override
    {
        return true;
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    uint32_t GetContentHash() override;
    uint64_t GetFeaturesID(const std::vector<std::string>& enabledFeature)
    {
        return GetFeaturesID(0, enabledFeature);
    }
    uint64_t GetFeaturesID(int shaderPassIndex, const std::vector<std::string>& enabledFeature);

    static const std::set<std::string>& GetEnabledFeatures()
    {
        return GetGlobalShaderFeature().GetEnabledFeatures();
    }
    static uint64_t GetEnabledFeaturesHash()
    {
        return GetGlobalShaderFeature().GetEnabledFeaturesHash();
    }
    static void EnableFeature(const char* name)
    {
        return GetGlobalShaderFeature().EnableFeature(name);
    }
    static void DisableFeature(const char* name)
    {
        return GetGlobalShaderFeature().DisableFeature(name);
    }

protected:
    struct GlobalShaderFeature
    {
        const std::set<std::string>& GetEnabledFeatures();
        uint64_t GetEnabledFeaturesHash();
        void EnableFeature(const char* name);
        void DisableFeature(const char* name);

    private:
        void Rehash();
        std::set<std::string> enabledFeatures;
        uint64_t setHash = 0;
    };
    static GlobalShaderFeature& GetGlobalShaderFeature();

    std::string shaderName;
    uint32_t contentHash = 0;

    struct ShaderPass
    {
        std::string name;
        std::unordered_map<std::string, uint64_t> featureToBitMask;
        Gfx::ShaderProgram* cachedShaderProgram = nullptr;
        uint64_t globalShaderFeaturesHash;
        std::unordered_map<uint64_t, std::unique_ptr<Gfx::ShaderProgram>> shaderPrograms;
    };
    std::vector<std::unique_ptr<ShaderPass>> shaderPasses;

    struct IncludedFiles
    {
        std::vector<std::filesystem::path> files;
        std::vector<size_t> lastWriteTime;
    } includedFiles;
};

class Shader : public ShaderBase
{
    DECLARE_ASSET();

public:
    Shader(){};
    Shader(
        const std::string& name,
        std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram,
        const UUID& uuid = UUID::GetEmptyUUID()
    )
        : ShaderBase(name, std::move(shaderProgram), uuid){};
    Shader(const char* path)
    {
        LoadFromFile(path);
    };
    bool LoadFromFile(const char* path) override;
    static void SetDefault(Shader* defaultShader);
    static Shader* GetDefault();

private:
    static Shader*& GetDefaultPrivate();
};

class ComputeShader : public ShaderBase
{
    DECLARE_ASSET();

public:
    ComputeShader(){};
    ComputeShader(
        const std::string& name,
        std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram,
        const UUID& uuid = UUID::GetEmptyUUID()
    )
        : ShaderBase(name, std::move(shaderProgram), uuid){};
    ComputeShader(const char* path)
    {
        LoadFromFile(path);
    };
    bool LoadFromFile(const char* path) override;
};
