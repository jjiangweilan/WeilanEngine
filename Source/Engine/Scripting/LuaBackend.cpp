#include "LuaBackend.hpp"
#include "Scripting/LuaBackend_Internal.hpp"
#include "ThirdParty/lua/lua.hpp"
#include <spdlog/spdlog.h>

LuaBackend::LuaBackend()
{
    if (L == nullptr)
    {
        L = luaL_newstate();
        luaL_openlibs(L);

        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "wl");

        lua_pushstring(L, "GameScript");
        LuaScript_LuaBinding().BindClass(L);
        lua_settable(L, -3);

        lua_pop(L, 1); // pop wl global table
    }
}

LuaBackend::~LuaBackend()
{
    if (L)
    {
        lua_close(L);
        L = nullptr;
    }
}

lua_State* LuaBackend::L = nullptr;
