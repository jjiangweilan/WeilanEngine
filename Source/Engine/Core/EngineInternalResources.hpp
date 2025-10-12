#pragma once

#include "Core/Graphics/Mesh.hpp"
#include "Rendering/Material.hpp"

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
    static Shader2& GetLineShader() { return *GetSingleton().lineShader; }
    static Shader2& GetTriangleShader() { return *GetSingleton().triangleShader; }
    static Shader2& GetJoltDebugShader() { return *GetSingleton().joltDebugShader; }
    static Texture& GetBlackTexture() { return *GetSingleton().blackTexture; }
    static Texture& GetWhiteTexture() { return *GetSingleton().whiteTexture; }

private:
    Models models;

    EngineInternalResources();
    Material* defaultMaterial;
    Material* defaultGridMaterial;

    // gizmos
    Shader2* lineShader;
    Shader2* triangleShader;

    // jolt debug
    Shader2* joltDebugShader;

    Texture* blackTexture;
    Texture* whiteTexture;

    static EngineInternalResources& GetSingleton();
};
