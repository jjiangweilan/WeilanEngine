#pragma once
#include "Core/Graphics/Shader.hpp"
#include <unordered_map>
namespace Engine::Rendering
{
class BuiltinShader
{
public:
    enum class ShaderName
    {
        StandardPBR,
        ImGui,
    };
    static std::unique_ptr<BuiltinShader> Init();
    static BuiltinShader* GetInstance();
    Shader* GetShader(ShaderName name);

private:
    static BuiltinShader* instance;
    BuiltinShader();
    std::unordered_map<ShaderName, std::unique_ptr<Shader>> shaders;
};
} // namespace Engine::Rendering
