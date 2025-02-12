#pragma once

#include "Scripting/LuaBackend_Internal.hpp"
#include "ThirdParty/lua/lua.hpp"
class LuaBackend
{
public:
    static LuaBackend* GetInstance() { return instance; }
    static lua_State* L;
    static UUID currentStateUUID;

    LuaBackend();
    ~LuaBackend();

    void Init(const char* projectAssetFolder);
    void Destroy();

private:
    static int EnginePrint(lua_State* L);
    static LuaBackend* instance;
};
