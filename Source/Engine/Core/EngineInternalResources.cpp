#include "EngineInternalResources.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

EngineInternalResources::EngineInternalResources()
{
    lineShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/LineShader.shad"));
    joltDebugShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/JoltDebugShader.shad"));
    triangleShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/TriangleShader.shad"));
    defaultMaterial =
        static_cast<Material*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Materials/Default.mat"));
}

EngineInternalResources& EngineInternalResources::GetSingleton()
{
    static EngineInternalResources singleton;
    return singleton;
}
