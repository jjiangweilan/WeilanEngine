#pragma once

#include "Rendering/Material.hpp"
#include "Rendering/Shader.hpp"
#include "Core/Graphics/Mesh.hpp"

class EngineInternalResources
{

public:
    struct Models
    {
        Mesh* sphere;
    };

    static void Init();

    static Models& GetModels()
    {
        return GetSingleton().models;
    }

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
    Models models;

    EngineInternalResources();
    Material* defaultMaterial;

    // gizmos
    Shader* lineShader;
    Shader* triangleShader;

    // jolt debug
    Shader* joltDebugShader;

    static EngineInternalResources& GetSingleton();
};
