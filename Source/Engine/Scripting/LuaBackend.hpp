#pragma once

#include "ThirdParty/lua/lua.hpp"
class LuaBackend
{
public:
    static lua_State* L;
    LuaBackend();
    ~LuaBackend();

    void Init(const char* projectAssetFolder);

private:
    static int EnginePrint(lua_State* L);
};
