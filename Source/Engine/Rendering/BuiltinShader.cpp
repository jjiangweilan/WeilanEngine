#include "BuiltinShader.hpp"
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
BuiltinShader::BuiltinShader()
{
    shaders[ShaderName::StandardPBR] = CreateShader("StandardPBR", "Assets/Shaders/Game/StandardPBR.shad");
}
Shader* BuiltinShader::GetShader(BuiltinShader::ShaderName name)
{
    return shaders[name].get();
};
std::unique_ptr<BuiltinShader> BuiltinShader::Init()
{
    if (instance == nullptr)
    {
        auto singleton = std::unique_ptr<BuiltinShader>(new BuiltinShader());
        instance = singleton.get();
        return singleton;
    }

    return nullptr;
}
BuiltinShader* BuiltinShader::GetInstance()
{
    return instance;
}

BuiltinShader* BuiltinShader::instance = nullptr;
} // namespace Engine::Rendering
