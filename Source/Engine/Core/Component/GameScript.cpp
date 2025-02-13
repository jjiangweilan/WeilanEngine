#include "GameScript.hpp"

#include "Scripting/LuaBackend.hpp"
#include "ThirdParty/lua/lauxlib.h"
#include "ThirdParty/lua/lua.h"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(GameScript, "8584BFED-B936-44D3-9011-9D492118A17C");

GameScript::GameScript() : Component(nullptr) {}
GameScript::GameScript(GameObject* gameObject) : Component(gameObject) {}

void GameScript::SetScript(ObjPtr<LuaScript> luaScript)
{
    const auto L = LuaBackend::L;
    this->luaScript = luaScript;

    if (luaRef != LUA_REFNIL && luaBackendUUID == LuaBackend::currentStateUUID)
    {
        OnStop();
        luaL_unref(L, LUA_REGISTRYINDEX, luaRef);
    }

    luaBackendUUID = LuaBackend::currentStateUUID;

    int luaClassRef = luaScript->GetLuaClassRef();
    if (luaClassRef != LUA_REFNIL)
    {
        void* m = lua_newuserdata(L, sizeof(LuaUserDataPack<GameScript*>));
        new (m) LuaUserDataPack<GameScript*>(LuaEngineUserDataType::RawPtr, this);

        lua_newtable(L);
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaClassRef);
            lua_setmetatable(L, 2);

            lua_pushvalue(L, 2);
            lua_setfield(L, 2, "__index");

            lua_pushvalue(L, 2);
            lua_setfield(L, 2, "__newindex");
        }
        lua_setmetatable(L, 1);
        luaRef = luaL_ref(L, LUA_REGISTRYINDEX);

        if (luaRef != LUA_REFNIL)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
            lua_getfield(L, 1, "Init");
            if (lua_isfunction(L, -1))
            {
                lua_pushvalue(L, 1);
                if (lua_pcall(L, 1, 0, 0))
                    SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
            }

            lua_pop(L, 1);
        }
    }
}

void GameScript::OnStart()
{
    LuaOnStart();
}

void GameScript::OnStop()
{
    LuaOnStop();
}

void GameScript::LuaOnStart()
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getfield(L, 1, "OnStart");
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, 1);
            if (lua_pcall(L, 1, 0, 0))
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 1);
    }
}

void GameScript::Tick()
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getfield(L, 1, "Tick");
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, 1);
            if (lua_pcall(L, 1, 0, 0) != 0)
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 1);
    }
}

void GameScript::LuaOnStop()
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getfield(L, 1, "OnStop");
        if (lua_isfunction(L, -1))
        {
            lua_pushvalue(L, 1);
            if (lua_pcall(L, 1, 0, 0))
                SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
        }

        lua_pop(L, 1);
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

    if (luaRef != LUA_REFNIL)
    {
        OnStop();
        luaL_unref(L, LUA_REGISTRYINDEX, luaRef);
    }
}

void GameScript::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    SERIALIZE(s, luaScript);

    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getmetatable(L, -1);

        lua_pushnil(L); // First key
        while (lua_next(L, -2) != 0)
        {
            // 'key' is at index -2 and 'value' at index -1.
            if (lua_isstring(L, -2))
            {
                printf("Key: %s, ", lua_tostring(L, -2));
            }
            else if (lua_isnumber(L, -2))
            {
                printf("Key: %g, ", lua_tonumber(L, -2));
            }

            if (lua_isstring(L, -1))
            {
                printf("Value: %s\n", lua_tostring(L, -1));
            }
            else if (lua_isboolean(L, -1))
            {
                printf("Value: %s\n", lua_toboolean(L, -1) ? "true" : "false");
            }
            else if (lua_isnumber(L, -1))
            {
                printf("Value: %g\n", lua_tonumber(L, -1));
            }
            else
            {
                printf("Value: (non-printable)\n");
            }

            lua_pop(L, 1); // Remove 'value', keep 'key'
                           //
                           // lua_getfield(L, 1, "Serialize");
                           //
                           // if (lua_isfunction(L, -1))
                           // {
                           //     lua_pushvalue(L, 1);
                           //     lua_pushlightuserdata(L, s);
                           //     if (lua_pcall(L, 2, 0, 0))
                           //         SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                           // }
        }

        int top = lua_gettop(L);

        lua_pop(L, 2);
    }
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
