#include "Shader.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "GfxDriver/GfxFactory.hpp"
#include <spdlog/spdlog.h>
namespace Engine
{
    Shader::Shader(const std::string& name, UniPtr<Gfx::ShaderProgram>&& shaderProgram) : shaderProgram(std::move(shaderProgram))
    {
        this->name = name;
    }
}
