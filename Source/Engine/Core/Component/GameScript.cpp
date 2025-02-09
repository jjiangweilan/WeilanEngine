#include "GameScript.hpp"

#include "Scripting/LuaBackend.hpp"
#include "ThirdParty/lua/lua.h"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(GameScript, "8584BFED-B936-44D3-9011-9D492118A17C");

GameScript::GameScript() : Component(nullptr) {}
GameScript::GameScript(GameObject* gameObject) : Component(gameObject) {}

void GameScript::SetScript(ObjPtr<LuaScript> luaScript)
{
    const auto L = LuaBackend::L;
    this->luaScript = luaScript;

    if (luaRef)
    {
        Destruct();
        luaL_unref(L, LUA_REGISTRYINDEX, luaRef);
    }

    int luaClassRef = luaScript->GetLuaClassRef();
    if (luaClassRef != LUA_REFNIL)
    {
        void* buf = lua_newuserdata(L, sizeof(ObjPtr<GameScript>));
        ObjPtr<GameScript>* obj = new (buf) ObjPtr<GameScript>(this);
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaClassRef);
        lua_setmetatable(L, -2);

        luaRef = luaL_ref(L, LUA_REGISTRYINDEX);
        Construct();
    }
}

void GameScript::Construct()
{
    const auto L = LuaBackend::L;

    if (luaRef)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getmetatable(L, -1);

        lua_getfield(L, -1, "Construct");
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, -3);
            if (lua_pcall(L, 1, 0, 0))
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 2);
    }
}

void GameScript::Tick()
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);

        lua_getmetatable(L, -1);

        lua_getfield(L, -1, "Tick");
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, -2);

            if (lua_pcall(L, 1, 0, 0) != 0)
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 2);
    }
}

void GameScript::Destruct()
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getmetatable(L, -1);

        lua_getfield(L, -1, "Destruct");
        if (lua_isfunction(L, -1))
        {
            if (lua_pcall(L, 1, 0, 0))
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 2);
    }
}

const std::string& GameScript::GetName()
{
    static std::string name = "GameScript";
    return name;
}

void GameScript::OnDestroy()
{
    const auto L = LuaBackend::L;

    if (luaRef)
    {
        Destruct();
        luaL_unref(L, LUA_REGISTRYINDEX, luaRef);
    }
}

void GameScript::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, luaScript);
}

void GameScript::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    DESERIALIZE(s, luaScript);
}

void GameScript::OnLoaded()
{
    if (luaScript != nullptr)
    {
        SetScript(luaScript);
    }
}
