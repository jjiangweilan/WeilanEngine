#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaHeaders.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"

// representing a lua class derived from GameScript
class LuaScript : public Asset
{
    DECLARE_EXTERNAL_ASSET();

public:
    int GetLuaClassRef();

    void LoadScript(const char* luaScriptPath);
    void ReloadScript();

private:
    AssetPath scriptAssetPath;

    int luaClassRef = LUA_REFNIL;
    UUID luaBackendUUID;
};
