#pragma once
#include "Core/Asset.hpp"
#include "ThirdParty/lua/lua.hpp"

// representing a lua class derived from GameScript
class LuaScript : public Asset
{
    DECLARE_ASSET();

public:

    // return a LuaRef representing as the instance from the lua class
    int Instantiate();

    void LoadScript(const char* luaScriptPath);

private:
    std::filesystem::path scriptAssetPath;

    int luaClassRef = LUA_REFNIL;
};
