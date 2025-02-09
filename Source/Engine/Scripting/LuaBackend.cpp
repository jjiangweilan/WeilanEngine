#include "LuaBackend.hpp"
#include "Scripting/LuaBackend_Internal.hpp"
#include "ThirdParty/lua/lua.h"
#include "ThirdParty/lua/lua.hpp"
#include <spdlog/spdlog.h>

LuaBackend::LuaBackend() {}

LuaBackend::~LuaBackend()
{
    if (L)
    {
        lua_close(L);
        L = nullptr;
    }
}

void LuaBackend::Init(const char* projectAssetFolder)
{
    L = luaL_newstate();
    luaL_openlibs(L);

    // set search path
    lua_getglobal(L, "package");
    lua_pushstring(L, "path");
    lua_pushstring(L, (std::string(projectAssetFolder) + "/?.lua").c_str());
    lua_settable(L, -3);
    lua_pop(L, 1);

    // redirect print
    static const struct luaL_Reg printlib[] = {
        {"print", EnginePrint},
        {NULL, NULL} /* end of array */
    };
    lua_getglobal(L, "_G");
    luaL_setfuncs(L, printlib, 0);
    lua_pop(L, 1);

    lua_newtable(L);

    LuaBinding<GameScript>::Bind(L);

    lua_setglobal(L, LuaNameSpace);
}

int LuaBackend::EnginePrint(lua_State* L)
{
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++)
    {
        if (lua_isstring(L, i))
        {
            spdlog::info(lua_tostring(L, i));
        }
    }

    return 0;
}

lua_State* LuaBackend::L = nullptr;

const char* const LuaNameSpace = "wl";
