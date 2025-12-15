#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/ThirdParty/lua/lua.hpp"

// representing a lua class derived from GameScript
class LuaScript : public Asset
{
    DECLARE_EXTERNAL_ASSET();

public:
    int GetLuaClassRef();

    void LoadScript(const char* luaScriptPath);
    void ReloadScript();

private:
    std::filesystem::path scriptAssetPath;

    int luaClassRef = LUA_REFNIL;
    UUID luaBackendUUID;
};
