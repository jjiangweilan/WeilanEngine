#include "LuaScript.hpp"

#include "Script/LuaBackend.hpp"
#include "Script/LuaWraps.hpp"
#define L LuaBackend::Instance()->GetL()

namespace Engine
{
#define GetLuaMember(luaRefName)                                                                                       \
    lua_getfield(L, -1, #luaRefName);                                                                                  \
    if (lua_isfunction(L, -1))                                                                                         \
        luaRef##luaRefName = luaL_ref(L, LUA_REGISTRYINDEX);                                                           \
    else                                                                                                               \
        lua_pop(L, 1);

#define SERIALIZE_MEMBERS() SERIALIZE_MEMBER(luaClassName);

LuaScript::LuaScript() : Component("LuaScript", nullptr) { SERIALIZE_MEMBERS(); }

LuaScript::LuaScript(GameObject* gameObject) : Component("LuaScript", gameObject) { SERIALIZE_MEMBERS(); }

void LuaScript::RefLuaClass(const char* luaClass)
{
    if (this->luaClassName == luaClass)
        return;
    this->luaClassName = luaClass;

    lua_getglobal(L, luaClass);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "New");
    lua_getglobal(L, luaClass);
    if (lua_isfunction(L, -2))
    {
        if (lua_pcall(L, 1, 1, 0) == 0)
        {
            GetLuaMember(Construct);
            GetLuaMember(Destruct);
            GetLuaMember(Tick);

            // gameObject
            GameObject** goPP = (GameObject**)lua_newuserdata(L, sizeof(void*));
            *goPP = gameObject.Get();
            luaL_setmetatable(L, "GameObject");
            lua_setfield(L, -2, "gameObject");

            luaRef = luaL_ref(L, LUA_REGISTRYINDEX);
            Construct();
        }
        else
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }
    else
        lua_pop(L, 2);

    lua_pop(L, 2); // pop lua_getglobal(L, luaClass);
}

void LuaScript::Construct()
{
    if (luaRefConstruct != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefConstruct);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        if (lua_pcall(L, 1, 0, 0) != 0)
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }
}

void LuaScript::Tick()
{
    if (luaRefTick != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefTick);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        if (lua_pcall(L, 1, 0, 0) != 0)
            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
    }
}

void LuaScript::Destruct()
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
} // namespace Engine
