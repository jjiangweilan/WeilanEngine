#include "EngineInternalShaders.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

EngineInternalShaders::EngineInternalShaders()
{
    lineShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/LineShader.shad"));
    joltDebugShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/JoltDebugShader.shad"));
    triangleShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/TriangleShader.shad"));
}

EngineInternalShaders& EngineInternalShaders::GetSingleton()
{
    static EngineInternalShaders singleton;
    return singleton;
}
