#pragma once
#include "Core/Object.hpp"
#include "GfxDriver/ShaderProgram.hpp"

class Shader2 : public Object
{
    DECLARE_OBJECT();

public:
    Shader2() : Object(), shaderProgram(nullptr) {}
    Shader2(Gfx::ShaderProgram* shaderProgram) : Object(), shaderProgram(shaderProgram) {}
    Shader2(const Shader2& other) : Object(), shaderProgram(other.shaderProgram) {}
    Shader2(Shader2&& other) : Object(std::move(other)), shaderProgram(other.shaderProgram) {}

    Shader2 operator=(const Shader2& other) = delete;
    bool operator==(const Shader2& other) const { return shaderProgram == other.shaderProgram; }
    Gfx::ShaderProgram* operator->() { return shaderProgram; }

    Gfx::ShaderProgram* GetShaderProgram() { return shaderProgram; }
    const std::string& GetName() const { return shaderProgram->GetName(); };

private:
    Gfx::ShaderProgram* shaderProgram = nullptr;
};
