#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include <spdlog/spdlog.h>
namespace Engine
{
Shader::Shader(const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid)
    : shaderProgram(std::move(shaderProgram))
{
    SetUUID(uuid);
    this->name = name;
}

Shader::Shader(const char* path)
{
    ShaderCompiler compiler;
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    compiler.Compile(ss.str(), true);

    shaderProgram =
        GetGfxDriver()
            ->CreateShaderProgram(path, &compiler.GetConfig(), compiler.GetVertexSPV(), compiler.GetFragSPV());

    this->name = path;
}

void Shader::Reload(Resource&& other)
{
    Shader* casted = static_cast<Shader*>(&other);
    Resource::Reload(std::move(other));
    shaderName = (std::move(casted->shaderName));
    shaderProgram = (std::move(casted->shaderProgram));
}

void Shader::Serialize(Serializer* s) const
{
    Resource::Serialize(s);
    s->Serialize("shaderName", shaderName);
}

void Shader::Deserialize(Serializer* s)
{
    Resource::Deserialize(s);
    s->Deserialize("shaderName", shaderName);
}
} // namespace Engine
