#pragma once
#include "Asset/Shader.hpp"
#include <functional>
#include <string>
#include <unordered_map>
namespace Engine::Rendering
{
using ShaderID = std::size_t;
class Shaders
{
public:
    Shaders();
    Shader* GetShader(ShaderID id);
    Shader* GetShader(const std::string& name);
    Shader* Add(const char* name, const std::filesystem::path& path);

    static ShaderID NameToID();

private:
    std::unordered_map<ShaderID, std::unique_ptr<Shader>> shaders;
    std::hash<std::string> nameToID;
};
} // namespace Engine::Rendering
