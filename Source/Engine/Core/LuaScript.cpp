#include "LuaScript.hpp"
#include "Scripting/LuaBackend.hpp"

DEFINE_ASSET(LuaScript, "656C3158-FDAB-4EB3-AED4-CC4DFACA46F8", "lua")

int LuaScript::Instantiate()
{
    const auto L = LuaBackend::L;
    if (luaClassRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaClassRef);
        lua_pushstring(L, "New");
        lua_gettable(L, -2);
        lua_pushvalue(L, -2);
        
        if (lua_pcall(L, 1, 0, 0))
        {
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
            lua_pop(L, 1);
            return LUA_REFNIL;
        }

        auto ref = luaL_ref(L, LUA_REGISTRYINDEX);
        return ref;
    }
    return LUA_REFNIL;
}

void LuaScript::LoadScript(const char* luaScriptPath)
{
    this->scriptAssetPath = luaScriptPath;
    this->SetName(luaScriptPath);

    auto L = LuaBackend::L;
    std::string script = fmt::format("local code = require '{}';return code or {}", luaScriptPath, "{}");

#define wllua_popall()                                                                                                 \
    int stacknum = lua_gettop(L);                                                                                      \
    lua_pop(L, stacknum);

    if (luaL_dostring(L, script.c_str()) != 0)
    {
        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        wllua_popall();
        return;
    }

    // only a single value should return from a game script
    int argnum = lua_gettop(L);
    if (argnum != 1)
    {
        spdlog::error(
            "Lua Error: a script is returning multiple values, a single table representing the lua class is expected"
        );
        lua_pop(L, argnum);
        return;
    }

    if (!lua_istable(L, -1))
    {
        wllua_popall();
        spdlog::error("Lua Error: a script is not returning a table");
        return;
    }

    // check if the script is a subclass of GameScript
    // if not then return
    if (lua_getmetatable(L, -1))
    {                                          /* does it have a metatable? */
        luaL_getmetatable(L, "wl.GameScript"); /* get correct metatable */
        if (!lua_rawequal(L, -1, -2))          /* not the same? */
        {
            wllua_popall();

            spdlog::critical("Internal Lua Error: wl.GameScript is not defined");
            return; /* value is a userdata with wrong metatable */
        }
        lua_pop(L, 2); /* remove both metatables */
    }

    lua_pushstring(L, "New");
    lua_gettable(L, -2);
    if (!lua_isfunction(L, -1))
    {
        wllua_popall();
        spdlog::critical("Internal Lua Error: wl.GameScript doesn't have a New function");
        return;
    }
    lua_pop(L, 1); // pop the function

    // Is this a valid lua GameScript
    luaClassRef = luaL_ref(L, LUA_REGISTRYINDEX);
}
