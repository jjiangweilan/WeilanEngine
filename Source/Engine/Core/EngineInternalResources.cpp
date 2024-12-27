#include "EngineInternalResources.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"
#include "Rendering/ShaderLibrary.hpp"
EngineInternalResources::EngineInternalResources()
{
    auto db = AssetDatabase::Singleton();
    lineShader = ShaderLibrary::GetShader(ShaderLibrary::LineShader);
    joltDebugShader = ShaderLibrary::GetShader(ShaderLibrary::JoltDebugShader);
    triangleShader = ShaderLibrary::GetShader(ShaderLibrary::TriangleShader);
    defaultMaterial = static_cast<Material*>(db->LoadAsset("_engine_internal/Materials/Default.mat"));
    models.sphere = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Sphere.glb")))->GetMeshes()[0].get();
    models.capsule = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Capsule.fbx")))->GetMeshes()[0].get();
    models.halfSphere =
        (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/HalfSphere.fbx")))->GetMeshes()[0].get();
    models.cylinder =
        (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Cylinder.fbx")))->GetMeshes()[0].get();
}

EngineInternalResources& EngineInternalResources::GetSingleton()
{
    static EngineInternalResources singleton;
    return singleton;
}
