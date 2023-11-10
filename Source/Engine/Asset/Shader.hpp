#pragma once
#include "Core/Asset.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include "Libs/Ptr.hpp"
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

    inline RefPtr<Gfx::ShaderProgram> GetDefaultShaderProgram()
    {
        return shaderPrograms[0];
    }

    Gfx::ShaderProgram* GetShaderProgram(uint64_t id)
    {
        return shaderPrograms[id].get();
    }

    uint64_t GetShaderProgramHash(const std::vector<std::string>& enabledFeature);

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

private:
    std::string shaderName;
    uint32_t contentHash = 0;

    std::unordered_map<uint64_t, std::unique_ptr<Gfx::ShaderProgram>> shaderPrograms;
};
} // namespace Engine
