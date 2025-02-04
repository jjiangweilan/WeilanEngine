#pragma once
#include "Core/Asset.hpp"
#include "ThirdParty/lua/lua.hpp"

// representing a lua class derived from GameScript
class LuaScript : public Asset
{
    DECLARE_ASSET();

public:
    int GetLuaClassRef() const { return luaClassRef; }

    void LoadScript(const char* luaScriptPath);

private:
    std::filesystem::path scriptAssetPath;

    int luaClassRef = LUA_REFNIL;
};
