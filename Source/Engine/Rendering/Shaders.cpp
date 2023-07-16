#include "Shaders.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/ShaderCompiler.hpp"
#include <fstream>
#include <sstream>

namespace Engine::Rendering
{

std::unique_ptr<Shader> CreateShader(const char* name, const std::filesystem::path& path)
{
    std::fstream input(path);
    std::stringstream codeStream;
    codeStream << input.rdbuf();
    std::string code = codeStream.str();

    ShaderCompiler shaderCompiler;
    try
    {
#if WE_DEBUG
        shaderCompiler.Compile(code, true);
#else
        shaderCompiler.Compile(code, false);
#endif
    }
    catch (ShaderCompiler::CompileError e)
    {
        return nullptr;
    }
    auto& vertSPV = shaderCompiler.GetVertexSPV();
    auto& fragSPV = shaderCompiler.GetFragSPV();

    auto shaderProgram = GetGfxDriver()->CreateShaderProgram(name, &shaderCompiler.GetConfig(), vertSPV, fragSPV);
    return std::make_unique<Shader>(name, std::move(shaderProgram));
}
Shaders::Shaders() : nameToID() {}

Shader* Shaders::Add(const char* name, const std::filesystem::path& path)
{
    ShaderID id = nameToID(name);
    assert(!shaders.contains(id));

    shaders[id] = CreateShader(name, "Assets/Shaders/Game/StandardPBR.shad");
    return shaders[id].get();
}
Shader* Shaders::GetShader(ShaderID id)
{
    return shaders[id].get();
};

Shader* Shaders::GetShader(const std::string& name)
{
    return shaders.at(nameToID(name)).get();
}
} // namespace Engine::Rendering
