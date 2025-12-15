#pragma once
#include "Engine/Core/Object.hpp"
#include "Engine/Driver/GfxDriver/ShaderProgram.hpp"
#include <unordered_map>

class Shader : public Object
{
    DECLARE_OBJECT();

public:
    Shader() : Object(), shaderProgram(nullptr) {}
    Shader(Gfx::ShaderProgram* shaderProgram) : Object(), shaderProgram(shaderProgram)
    {
        SetName(shaderProgram->GetName());
    }
    Shader(const Shader& other) : Object(other), shaderProgram(other.shaderProgram) { SetName(other.name); }
    Shader(Shader&& other) : Object(std::move(other)), shaderProgram(other.shaderProgram) {}

    Shader operator=(const Shader& other) = delete;
    bool operator==(const Shader& other) const { return shaderProgram == other.shaderProgram; }
    Gfx::ShaderProgram* operator->() { return shaderProgram; }

    Gfx::ShaderProgram* GetShaderProgram() { return shaderProgram; }
    const std::string& GetName() const { return shaderProgram->GetName(); };
    void ReplaceShader(Gfx::ShaderProgram* shader) { this->shaderProgram = shader; }
    int GetSet(const std::string& name);
    int GetSet(Gfx::DescriptorSetSemantics setSlot);

private:
    Gfx::ShaderProgram* shaderProgram = nullptr;
};
