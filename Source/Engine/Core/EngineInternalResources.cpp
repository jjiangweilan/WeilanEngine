#include "EngineInternalResources.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

EngineInternalResources::EngineInternalResources()
{
    auto db = AssetDatabase::Singleton();
    lineShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/LineShader.shad"));
    joltDebugShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/JoltDebugShader.shad"));
    triangleShader = static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/TriangleShader.shad"));
    defaultMaterial = static_cast<Material*>(db->LoadAsset("_engine_internal/Materials/Default.mat"));
    lightProbeFieldGBufferShader =
        static_cast<Shader*>(db->LoadAsset("_engine_internal/Shaders/LightFieldProbes/GBufferGeneration.shad"));
}

EngineInternalResources& EngineInternalResources::GetSingleton()
{
    static EngineInternalResources singleton;
    return singleton;
}
