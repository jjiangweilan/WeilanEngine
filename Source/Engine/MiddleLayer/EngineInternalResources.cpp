#include "EngineInternalResources.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/Object/Mesh/Model.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
EngineInternalResources::EngineInternalResources()
{
    auto db = AssetDatabase::Singleton();
    lineShader = ShaderLibrary::GetShader(Shaders::LineShader);
    joltDebugShader = ShaderLibrary::GetShader(Shaders::JoltDebugShader);
    triangleShader = ShaderLibrary::GetShader(Shaders::TriangleShader);
    defaultMaterial = static_cast<Material*>(db->LoadAsset("_engine_internal/Materials/Default.mat"));
    defaultMaterial->SetShader(Shaders::SceneLit);
    defaultMaterial->SetFlags(AssetStateFlags::DontSave);
    defaultGridMaterial = static_cast<Material*>(db->LoadAsset("_engine_internal/Materials/PrimitiveGrid.mat"));
    defaultGridMaterial->SetShader(ShaderLibrary::GetShader(Shaders::PrimitiveShape));
    defaultGridMaterial->SetFlags(AssetStateFlags::DontSave);
    models.sphere = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Sphere.fbx")))->GetMeshes()[0].get();
    models.capsule = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Capsule.fbx")))->GetMeshes()[0].get();
    models.cube = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Cube.fbx")))->GetMeshes()[0].get();
    models.plane = (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Plane.fbx")))->GetMeshes()[0].get();
    models.halfSphere =
        (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/HalfSphere.fbx")))->GetMeshes()[0].get();
    models.cylinder =
        (static_cast<Model*>(db->LoadAsset("_engine_internal/Models/Cylinder.fbx")))->GetMeshes()[0].get();
    blackTexture = static_cast<Texture*>(db->LoadAsset("_engine_internal/Textures/black.png"));
    whiteTexture = static_cast<Texture*>(db->LoadAsset("_engine_internal/Textures/white.png"));
}

EngineInternalResources& EngineInternalResources::GetSingleton()
{
    static EngineInternalResources singleton;
    return singleton;
}
