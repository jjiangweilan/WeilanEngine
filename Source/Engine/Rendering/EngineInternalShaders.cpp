#include "EngineInternalShaders.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

EngineInternalShaders::EngineInternalShaders()
{
    lineShader =
        static_cast<Shader*>(AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/LineShader.shad"));
}

EngineInternalShaders& EngineInternalShaders::GetSingleton()
{
    static EngineInternalShaders singleton;
    return singleton;
}
