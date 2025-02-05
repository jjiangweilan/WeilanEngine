#pragma once
#include "Core/Component/GameScript.hpp"
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lua.hpp"

namespace LuaBinderHelper
{

/****** Type Processing Dispatchers *******/

struct ProcessArgs
{};

/****************************************/
} // namespace LuaBinderHelper

template <class T>
class LuaBinder
{
public:
    LuaBinder(lua_State* L, const char* name)
    {
        this->L = L;
        this->name = name;
        luaL_newmetatable(L, name);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */
    }

    ~LuaBinder()
    {
        lua_pushstring(L, name);
        lua_settable(L, -3);
    }

    template <class R, class... Args>
    void BindFunction(const char* name, R (T::*f)(Args...))
    {
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                T* v = (T*)lua_touserdata(L, -1);
                CallMemberFunc<T, Args...>(v, FF);

                return 0;
            };
        };

        lua_pushcfunction(L, &Wrap::cfunc);
        lua_pushstring(L, name);
        lua_settable(L, -3);
    }

    template <class R, class... Args>
    void BindFunction(const char* name, R (T::*f)(Args...) const)
    {
        BindFunction(name, reinterpret_cast<R (T::*)(Args...)>(f));
    }

private:
    lua_State* L;
    const char* name;
    template <class Type>
    static std::remove_reference_t<Type> ProcessArg()
    {
        return std::remove_reference_t<Type>{};
    }

    /****** Type Processing *******/
    template <class ObjType, class... Args>
    static void CallMemberFunc(ObjType* v, auto f)
    {
        (v->*f)(ProcessArg<Args>()...);
    }
};

class LuaBindings
{
public:
    // this function will leave a table on stack
    void BindClasses(lua_State* L)
    {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "wl");

        LuaBinder<GameScript> gameScript(L, "GameScript");
        gameScript.BindFunction("Print", &GameScript::Print);

        lua_pop(L, 1); // pop wl global table
    }
};
