#pragma once

#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"

class EngineInternalResources
{

public:
    struct Models
    {
        Mesh* sphere;
        Mesh* capsule;
        Mesh* halfSphere;
        Mesh* cylinder;
        Mesh* cube;
        Mesh* plane;
    };

    static void Init();

    static Submesh* GetSphereMesh() { return GetSingleton().models.sphere->GetSubmesh(0); }
    static Submesh* GetCapsuleMesh() { return GetSingleton().models.capsule->GetSubmesh(0); }
    static Submesh* GetHalfSphereMesh() { return GetSingleton().models.halfSphere->GetSubmesh(0); }
    static Submesh* GetCylinderMesh() { return GetSingleton().models.cylinder->GetSubmesh(0); }
    static Submesh* GetCubeMesh() { return GetSingleton().models.cube->GetSubmesh(0); }
    static Submesh* GetPlaneMesh() { return GetSingleton().models.plane->GetSubmesh(0); }
    static Models& GetModels() { return GetSingleton().models; }
    static Material* GetDefaultMaterial() { return GetSingleton().defaultMaterial; }
    static Material* GetDefaultGridMaterial() { return GetSingleton().defaultGridMaterial; }
    static Shader& GetLineShader() { return *GetSingleton().lineShader; }
    static Shader& GetTriangleShader() { return *GetSingleton().triangleShader; }
    static Shader& GetJoltDebugShader() { return *GetSingleton().joltDebugShader; }
    static Texture& GetBlackTexture() { return *GetSingleton().blackTexture; }
    static Texture& GetWhiteTexture() { return *GetSingleton().whiteTexture; }

private:
    Models models;

    EngineInternalResources();
    Material* defaultMaterial;
    Material* defaultGridMaterial;

    // gizmos
    Shader* lineShader;
    Shader* triangleShader;

    // jolt debug
    Shader* joltDebugShader;

    Texture* blackTexture;
    Texture* whiteTexture;

    static EngineInternalResources& GetSingleton();
};
