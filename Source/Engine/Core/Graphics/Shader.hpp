#pragma once
#include "Core/Resource.hpp"
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
class Shader : public Resource
{
public:
    Shader(
        const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid = UUID::empty
    );
    Shader(const char* path);
    void Reload(Resource&& loaded) override;
    ~Shader() override {}

    inline RefPtr<Gfx::ShaderProgram> GetShaderProgram()
    {
        return shaderProgram;
    }
    inline const Gfx::ShaderConfig& GetDefaultShaderConfig()
    {
        return shaderProgram->GetDefaultShaderConfig();
    }

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    std::string shaderName;

    std::unique_ptr<Gfx::ShaderProgram> shaderProgram;
};
} // namespace Engine
