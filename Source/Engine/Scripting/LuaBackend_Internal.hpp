#pragma once
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lua.hpp"

class LuaScript_LuaBinding
{
public:
    void BindClass(lua_State* L)
    {
        luaL_newmetatable(L, "wl.GameScript");
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */

        const luaL_Reg funcs[] = {{"New", New}, {"GetComponent", GetComponent}, {nullptr, nullptr}};
        luaL_setfuncs(L, funcs, 0);
    }

    static int New(lua_State* L)
    {
        lua_newtable(L);
        luaL_setmetatable(L, "wl.GameScript");

        return 1;
    }

    static int GetComponent(lua_State* L)
    {
        spdlog::info("Call GetComponent in lua");
        return 0;
    }
};

class GameObject_LuaBinding
{
public:
    void Bind(GameObject* self, lua_State* L)
    {
        GameObject** ptr = (GameObject**)lua_newuserdata(L, sizeof(void*));
        *ptr = self;
    }

private:
};
