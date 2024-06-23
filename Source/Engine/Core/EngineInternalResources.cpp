#include "EngineInternalResources.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Model.hpp"

EngineInternalResources::EngineInternalResources()
{
    auto db = AssetDatabase::Singleton();
    lineShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/LineShader.shad"));
    joltDebugShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/JoltDebugShader.shad"));
    triangleShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/TriangleShader.shad"));
    defaultMaterial = static_cast<Material*>(db->LoadAsset("_engine_internal/Materials/Default.mat"));

    models.sphere = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/sphere.glb")))->GetMeshes()[0].get();
}

EngineInternalResources& EngineInternalResources::GetSingleton()
{
    static EngineInternalResources singleton;
    return singleton;
}
