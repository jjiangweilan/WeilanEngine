#include "Shader.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include <spdlog/spdlog.h>
namespace Engine
{
DEFINE_ASSET(Shader, "41EF74E2-6DAF-4755-A385-ABFCC4E83147", "shad");

Shader::Shader(const std::string& name, std::unique_ptr<Gfx::ShaderProgram>&& shaderProgram, const UUID& uuid)
{
    shaderPrograms[0] = std::move(shaderProgram);
    SetUUID(uuid);
    this->name = name;
}

Shader::Shader(const char* path)
{
    LoadFromFile(path);
}

void Shader::Reload(Asset&& other)
{
    Shader* casted = static_cast<Shader*>(&other);
    Asset::Reload(std::move(other));
    shaderName = (std::move(casted->shaderName));
    shaderPrograms = (std::move(casted->shaderPrograms));
}

void Shader::Serialize(Serializer* s) const
{
    Asset::Serialize(s);
    s->Serialize("shaderName", shaderName);
}

void Shader::Deserialize(Serializer* s)
{
    Asset::Deserialize(s);
    s->Deserialize("shaderName", shaderName);
}

bool Shader::LoadFromFile(const char* path)
{
    ShaderCompiler compiler;
    std::fstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    try
    {
        compiler.Compile(ss.str(), true);
        shaderPrograms[0] =
            GetGfxDriver()
                ->CreateShaderProgram(path, compiler.GetConfig(), compiler.GetVertexSPV(), compiler.GetFragSPV());
        this->name = compiler.GetName();
        contentHash += 1;
    }
    catch (const std::exception& e)
    {
        SPDLOG_ERROR("{}", e.what());
    }

    return true;
}

uint32_t Shader::GetContentHash()
{
    return contentHash;
}

} // namespace Engine
