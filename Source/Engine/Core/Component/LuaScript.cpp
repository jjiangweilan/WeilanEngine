#include "LuaScript.hpp"

#include "Script/LuaBackend.hpp"
#define L LuaBackend::Instance()->GetL()

namespace Engine
{
#define GetLuaMember(luaRefName) \
    lua_getfield(L, -1, #luaRefName); \
    if (lua_isfunction(L, -1)) luaRef##luaRefName = luaL_ref(L, LUA_REGISTRYINDEX); \
    else lua_pop(L, 1);

    LuaScript::LuaScript() : Component("LuaScript", nullptr)
    {

    }

    LuaScript::LuaScript(GameObject* gameObject): Component("LuaScript", gameObject)
    {

    }

    void LuaScript::RefLuaClass(const char* luaClass)
    {
        if (this->luaClassName == luaClass) return;
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
            lua_pcall(L, 1, 1, 0);

            if (lua_istable(L, -1))
            {
                GetLuaMember(Construct);
                GetLuaMember(Destruct);
                GetLuaMember(Tick);

                luaRef = luaL_ref(L, -1);
                Construct();
            } else lua_pop(L, 1);
        }
        else lua_pop(L, 1);

        lua_pop(L, 1); // pop lua_getglobal(L, luaClass);
    }

    void LuaScript::Construct()
    {
        if (luaRefConstruct != LUA_REFNIL)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefConstruct);
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
            lua_pcall(L, 1, 0, 0); // no need to test if it's function, it's tested when we get the reference
        }
    }

    void LuaScript::Tick()
    {
        if (luaRefTick != LUA_REFNIL)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefTick);
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
            lua_pcall(L, 1, 0, 0); // no need to test if it's function, it's tested when we get the reference
        }
    }

    void LuaScript::Destruct()
    {
        if (luaRefTick != LUA_REFNIL)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRefDestruct);
            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
            lua_pcall(L, 1, 0, 0); // no need to test if it's function, it's tested when we get the reference
        }
    }
}
