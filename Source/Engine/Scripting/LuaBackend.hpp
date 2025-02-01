#pragma once

#include "ThirdParty/lua/lua.hpp"
class LuaBackend
{
public:
    static lua_State* L;
    LuaBackend();
    ~LuaBackend();

private:

    /**** Test *****/
    int luaRef;
    void InstantiateScript(const char* scriptPath);
    /***************/
};
