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

    inline RefPtr<Gfx::ShaderProgram> GetShaderProgram()
    {
        return shaderProgram;
    }

    inline const Gfx::ShaderConfig& GetDefaultShaderConfig()
    {
        return shaderProgram->GetDefaultShaderConfig();
    }

    bool IsExternalAsset() override
    {
        return true;
    }

    // return false if loading failed
    bool LoadFromFile(const char* path) override;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    std::string shaderName;

    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
};
} // namespace Engine
