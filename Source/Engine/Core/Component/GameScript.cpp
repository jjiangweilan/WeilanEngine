#include "GameScript.hpp"

#include "ThirdParty/lua/lua.h"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(GameScript, "8584BFED-B936-44D3-9011-9D492118A17C");

#define GetLuaMember(luaRefName)                                                                                       \
    lua_getfield(L, -1, #luaRefName);                                                                                  \
    if (lua_isfunction(L, -1))                                                                                         \
        luaRef##luaRefName = luaL_ref(L, LUA_REGISTRYINDEX);                                                           \
    else                                                                                                               \
        lua_pop(L, 1);

#define SERIALIZE_MEMBERS() SERIALIZE_MEMBER(luaClassName);

GameScript::GameScript() : Component(nullptr) {}

GameScript::GameScript(GameObject* gameObject) : Component(gameObject) {}

void GameScript::SetScript(const char* scriptPath)
{
    std::string script = fmt::format("local code = require '{}';return code or {}", scriptPath, "{}");

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

    if (lua_pcall(L, 0, 1, 0) != 0)
    {
        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        wllua_popall();
        return;
    }

    luaRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
    lua_pushstring(L, "GetComponent");
    lua_gettable(L, -2);
    lua_pcall(L, 0, 0, 0);

    wllua_popall();
}

void GameScript::Construct()
{
    if (luaRefConstruct != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefConstruct);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        if (lua_pcall(L, 1, 0, 0) != 0)
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }
}

void GameScript::Tick()
{
    if (luaRefTick != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefTick);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        if (lua_pcall(L, 1, 0, 0) != 0)
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }
}

void GameScript::Destruct()
{
    if (luaRefTick != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefDestruct);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        if (lua_pcall(L, 1, 0, 0) != 0)
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }

    luaL_unref(L, LUA_REGISTRYINDEX, luaRef);
    luaL_unref(L, LUA_REGISTRYINDEX, luaRefConstruct);
    luaL_unref(L, LUA_REGISTRYINDEX, luaRefDestruct);
    luaL_unref(L, LUA_REGISTRYINDEX, luaRefTick);
}

const std::string& GameScript::GetName()
{
    static std::string name = "LuaScript";
    return name;
}
