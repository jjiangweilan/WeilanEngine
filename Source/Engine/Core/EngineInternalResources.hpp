#pragma once

#include "Asset/Material.hpp"
#include "Asset/Shader.hpp"

class EngineInternalResources
{
public:
    static void Init();

    static Material* GetDefaultMaterial()
    {
        return GetSingleton().defaultMaterial;
    }
    static Shader* GetLineShader()
    {
        return GetSingleton().lineShader;
    }

    static Shader* GetTriangleShader()
    {
        return GetSingleton().triangleShader;
    }

    static Shader* GetJoltDebugShader()
    {
        return GetSingleton().joltDebugShader;
    }

private:
    EngineInternalResources();
    Material* defaultMaterial;
    Shader* lineShader;
    Shader* triangleShader;
    Shader* joltDebugShader;
    static EngineInternalResources& GetSingleton();
};
