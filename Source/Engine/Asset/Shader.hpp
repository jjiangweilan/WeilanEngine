#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Libs/Ptr.hpp"
#include <set>
#include <string>
namespace Engine
{
namespace Gfx
{
class ShaderProgram;
class ShaderLoader;
} // namespace Gfx
class Shader : public Asset
{
    DECLARE_ASSET();

public:
    Shader(){};
    Shader(
        const std::string& name,
        std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram,
        const UUID& uuid = UUID::GetEmptyUUID()
    );
    Shader(const char* path);
    void Reload(Asset&& loaded) override;
    ~Shader() override {}

    // get a shader program with enabled global shader features
    Gfx::ShaderProgram* GetDefaultShaderProgram();

    Gfx::ShaderProgram* GetShaderProgram(const std::vector<std::string>& enabledFeature);

    uint64_t GetShaderProgramID();

    inline const Gfx::ShaderConfig& GetDefaultShaderConfig()
    {
        return shaderPrograms[0]->GetDefaultShaderConfig();
    }

    bool IsExternalAsset() override
    {
        return true;
    }

    // return false if loading failed
    bool LoadFromFile(const char* path) override;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    uint32_t GetContentHash() override;

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

private:
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
    std::unordered_map<std::string, uint64_t> featureToBitMask;
    Gfx::ShaderProgram* cachedShaderProgram = nullptr;
    uint64_t globalShaderFeaturesHash;

    std::unordered_map<uint64_t, std::unique_ptr<Gfx::ShaderProgram>> shaderPrograms;
};
} // namespace Engine
