#pragma once

#include "Core/Graphics/Mesh.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Shader.hpp"

class EngineInternalResources
{

public:
    struct Models
    {
        Mesh* sphere;
        Mesh* capsule;
        Mesh* halfSphere;
        Mesh* cylinder;
    };

    static void Init();

    static Submesh* GetCapsuleMesh() { return GetSingleton().models.capsule->GetSubmesh(0); }
    static Submesh* GetHalfSphereMesh() { return GetSingleton().models.halfSphere->GetSubmesh(0); }
    static Submesh* GetCylinderMesh() { return GetSingleton().models.cylinder->GetSubmesh(0); }
    static Models& GetModels() { return GetSingleton().models; }
    static Material* GetDefaultMaterial() { return GetSingleton().defaultMaterial; }
    static Shader* GetLineShader() { return GetSingleton().lineShader; }
    static Shader* GetTriangleShader() { return GetSingleton().triangleShader; }
    static Shader* GetJoltDebugShader() { return GetSingleton().joltDebugShader; }

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
