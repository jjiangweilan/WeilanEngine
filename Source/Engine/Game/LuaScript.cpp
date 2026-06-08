#include "LuaScript.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBackend.hpp"

#include <unordered_set>

namespace
{
bool InheritsFromGameScript(lua_State* L, int classIndex)
{
    const int initialStackTop = lua_gettop(L);
    if (classIndex < 0 && classIndex > LUA_REGISTRYINDEX)
    {
        classIndex = initialStackTop + classIndex + 1;
    }

    luaL_getmetatable(L, "GameScript");
    if (!lua_istable(L, -1))
    {
        lua_settop(L, initialStackTop);
        return false;
    }

    const int gameScriptMetatableIndex = lua_gettop(L);
    lua_pushvalue(L, classIndex);

    std::unordered_set<const void*> visitedClasses;
    while (lua_istable(L, -1))
    {
        if (!visitedClasses.insert(lua_topointer(L, -1)).second)
        {
            break;
        }

        if (!lua_getmetatable(L, -1))
        {
            break;
        }

        if (lua_rawequal(L, -1, gameScriptMetatableIndex))
        {
            lua_settop(L, initialStackTop);
            return true;
        }

        lua_pushliteral(L, "__index");
        lua_rawget(L, -2);
        lua_remove(L, -2);
        lua_remove(L, -2);
    }

    lua_settop(L, initialStackTop);
    return false;
}
}

DEFINE_ASSET(LuaScript, "656C3158-FDAB-4EB3-AED4-CC4DFACA46F8", "lua")

void LuaScript::LoadScript(const char* luaScriptPath)
{
    this->scriptAssetPath = luaScriptPath;
    this->SetName(luaScriptPath);

    auto L = LuaBackend::L;
    luaBackendUUID = LuaBackend::currentStateUUID;
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

    if (!InheritsFromGameScript(L, -1))
    {
        wllua_popall();
        spdlog::critical("Lua Error: script does not inherit from GameScript");
        return;
    }

    lua_pushstring(L, "New");
    lua_gettable(L, -2);
    if (!lua_isfunction(L, -1))
    {
        wllua_popall();
        spdlog::critical("Internal Lua Error: GameScript doesn't have a New function");
        return;
    }
    lua_pop(L, 1); // pop the function

    // Is this a valid lua GameScript
    if (luaClassRef != LUA_REFNIL)
    {
        luaL_unref(L, LUA_REGISTRYINDEX, luaClassRef);
    }
    luaClassRef = luaL_ref(L, LUA_REGISTRYINDEX);
}

int LuaScript::GetLuaClassRef()
{
    if (luaBackendUUID != LuaBackend::currentStateUUID)
    {
        LoadScript(scriptAssetPath.string().c_str());
    }
    return luaClassRef;
}

void LuaScript::ReloadScript()
{
    LoadScript(scriptAssetPath.string().c_str());
}
