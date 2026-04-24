#pragma once

#include "Engine/Runtime/System/ScriptingBackend/LuaBindings.hpp"
#include "Engine/ThirdParty/lua/lua.hpp"

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

// extern "C" {
//     int luaopen_WeilanEngine(lua_State* L);
// }
