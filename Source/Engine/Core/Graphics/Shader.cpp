#include "Shader.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>
namespace Engine
{
    Shader::Shader(const std::string& name, UniPtr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid) : AssetObject(uuid), shaderProgram(std::move(shaderProgram))
    {
        SERIALIZE_MEMBER(shaderName);
        this->name = name;
    }

    void Shader::Reload(AssetObject&& other)
    {
        Shader* casted = static_cast<Shader*>(&other);
        AssetObject::Reload(std::move(other));
        shaderName = (std::move(casted->shaderName));
        shaderProgram = (std::move(casted->shaderProgram));
    }
}
