#include "GameScript.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
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

                lua_getmetatable(L, -1);

                lua_pushnil(L); // First key
                while (lua_next(L, -2) != 0)
                {
                    std::string key = "";
                    int keyType = lua_type(L, -2);
                    if (keyType == LUA_TSTRING)
                    {
                        key = lua_tostring(L, -2);
                    }
                    else if (keyType == LUA_TNUMBER)
                    {
                        key = std::to_string(lua_tonumber(L, -2));
                    }
                    if (key[0] != '_' && key[1] != '_')
                    {
                        serializationValKeys.push_back(key);
                    }

                    lua_pop(L, 1); // Remove 'value', keep 'key'
                }

                lua_pop(L, 1); // pop the metatable
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

    auto subs = s->CreateSubserializer();
    LuaSerialize(subs.get());
    s->AppendSubserializer("LuaData", subs.get());
}

void GameScript::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    UUID luaScriptUUID;
    s->Deserialize("luaScript", luaScriptUUID);

    if (luaScriptUUID != UUID::GetEmptyUUID())
    {
        luaScript = AssetDatabase::Singleton()->LoadAssetByID(luaScriptUUID);

        if (luaScript != nullptr)
        {
            SetScript(luaScript);
            auto subs = s->CreateSubdeserializer("LuaData");
            LuaDeserialize(subs.get());
        }
    }
}

void GameScript::LuaSerialize(Serializer* s) const
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getmetatable(L, -1);

        for (const auto& key : serializationValKeys)
        {
            lua_getfield(L, -1, key.c_str());

            int valType = lua_type(L, -1);
            if (valType == LUA_TSTRING)
            {
                std::string val = lua_tostring(L, -1);
                s->Serialize(key, val);
            }
            else if (valType == LUA_TBOOLEAN)
            {
                bool val = lua_toboolean(L, -1);
                s->Serialize(key, val);
            }
            else if (lua_isnumber(L, -1))
            {
                float val = lua_tonumber(L, -1);
                s->Serialize(key, val);
            }
            else if (lua_isuserdata(L, -1))
            {
                lua_getfield(L, -1, "Serialize");

                bool serializeFailed = false;
                if (lua_isfunction(L, -1))
                {
                    lua_pushvalue(L, -2);

                    auto subs = s->CreateSubserializer();
                    // prepare serializer
                    LuaUserDataPack<Serializer*>* d =
                        (LuaUserDataPack<Serializer*>*)lua_newuserdata(L, sizeof(LuaUserDataPack<Serializer*>));
                    d->dataType =
                        LuaEngineUserDataType::Value; // Value? yes, we are pass the pointer as value, if it's a RawPtr
                                                      // type LuaBackend will deference it to get the actual `value`
                    d->val = subs.get();

                    if (lua_pcall(L, 2, 0, 0))
                        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));

                    s->AppendSubserializer(key, subs.get());
                }
                else
                {
                    serializeFailed = true;
                    lua_pop(L, 1); // pop the nil field
                }

                if (serializeFailed)
                {
                    lua_getfield(L, -1, "SerializeTo");

                    if (lua_isfunction(L, -1))
                    {
                        lua_pushvalue(L, -2);
                        lua_pushstring(L, key.c_str());

                        // prepare serializer
                        LuaUserDataPack<Serializer*>* d =
                            (LuaUserDataPack<Serializer*>*)lua_newuserdata(L, sizeof(LuaUserDataPack<Serializer*>));
                        d->dataType = LuaEngineUserDataType::Value;
                        d->val = s;

                        if (lua_pcall(L, 3, 0, 0))
                            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                    }
                    else
                        lua_pop(L, 1);
                }
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 2);
    }
}
void GameScript::LuaDeserialize(Serializer* s)
{
    const auto L = LuaBackend::L;

    if (luaRef != LUA_REFNIL)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
        lua_getmetatable(L, -1);

        for (const auto& key : serializationValKeys)
        {
            lua_getfield(L, -1, key.c_str());
            int valType = lua_type(L, -1);

            if (valType == LUA_TSTRING)
            {
                std::string val;
                s->Deserialize(key, val);
                lua_pushlstring(L, val.c_str(), val.length());
                lua_setfield(L, 1, key.c_str());
            }
            else if (valType == LUA_TBOOLEAN)
            {
                bool val;
                s->Deserialize(key, val);
                lua_pushboolean(L, val);
                lua_setfield(L, 1, key.c_str());
            }
            else if (lua_isnumber(L, -1))
            {
                float val = lua_tonumber(L, -1);
                s->Deserialize(key, val);
                lua_pushnumber(L, val);
                lua_setfield(L, 1, key.c_str());
            }
            else if (lua_isuserdata(L, -1))
            {
                lua_getfield(L, -1, "Deserialize");

                bool deserializeFailed = false;
                if (lua_isfunction(L, -1))
                {
                    lua_pushvalue(L, -2);

                    auto subs = s->CreateSubdeserializer(key);
                    // prepare serializer
                    LuaUserDataPack<Serializer*>* d =
                        (LuaUserDataPack<Serializer*>*)lua_newuserdata(L, sizeof(LuaUserDataPack<Serializer*>));
                    d->dataType = LuaEngineUserDataType::Value;
                    d->val = subs.get();

                    if (lua_pcall(L, 2, 0, 0))
                        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                }
                else
                {
                    deserializeFailed = true;
                    lua_pop(L, 1); // pop the nil field
                }

                if (deserializeFailed)
                {
                    lua_getfield(L, -1, "DeserializeTo");

                    if (lua_isfunction(L, -1))
                    {
                        lua_pushvalue(L, -2);
                        lua_pushstring(L, key.c_str());

                        // prepare serializer
                        LuaUserDataPack<Serializer*>* d =
                            (LuaUserDataPack<Serializer*>*)lua_newuserdata(L, sizeof(LuaUserDataPack<Serializer*>));
                        d->dataType = LuaEngineUserDataType::Value;
                        d->val = s;

                        if (lua_pcall(L, 3, 0, 0))
                            SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                    }
                    else
                        lua_pop(L, 1);
                }
            }

            lua_pop(L, 1);
        }

        lua_pop(L, 2);
    }
}

std::unique_ptr<Component> GameScript::Clone(GameObject& owner)
{
    auto newScript = std::make_unique<GameScript>();
    newScript->luaScript = luaScript;
    newScript->luaBackendUUID = luaBackendUUID;
    newScript->serializationValKeys = serializationValKeys;

    return newScript;
}
