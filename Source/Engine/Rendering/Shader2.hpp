#pragma once
#include "Core/Object.hpp"
#include "GfxDriver/ShaderProgram.hpp"
#include <unordered_map>

class Shader2 : public Object
{
    DECLARE_OBJECT();

public:
    Shader2() : Object(), shaderProgram(nullptr) {}
    Shader2(Gfx::ShaderProgram* shaderProgram) : Object(), shaderProgram(shaderProgram)
    {
        SetName(shaderProgram->GetName());
    }
    Shader2(const Shader2& other) : Object(other), shaderProgram(other.shaderProgram) { SetName(other.name); }
    Shader2(Shader2&& other) : Object(std::move(other)), shaderProgram(other.shaderProgram) {}

    Shader2 operator=(const Shader2& other) = delete;
    bool operator==(const Shader2& other) const { return shaderProgram == other.shaderProgram; }
    Gfx::ShaderProgram* operator->() { return shaderProgram; }

    Gfx::ShaderProgram* GetShaderProgram() { return shaderProgram; }
    const std::string& GetName() const { return shaderProgram->GetName(); };
    void ReplaceShader(Gfx::ShaderProgram* shader) { this->shaderProgram = shader; }
    int GetSet(const std::string& name);
    int GetSet(Gfx::DescriptorSetSemantics setSlot);

private:
    Gfx::ShaderProgram* shaderProgram = nullptr;
};
