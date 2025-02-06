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
    LuaBinder(lua_State* L) { this->L = L; }

    LuaBinder<T>& Begin(const char* name)
    {
        this->name = name;

        luaL_newmetatable(L, name);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */

        return *this;
    }

    LuaBinder<T>& End()
    {
        lua_pushstring(L, name);
        lua_pushvalue(L, -2);
        lua_settable(L, -4); /* set the global wl table */
        lua_pop(L, 1);       /* pop the metatable */

        return *this;
    }

    template <class R, class... Args>
    LuaBinder<T>& BindMemFn(const char* name, R (T::*f)(Args...))
    {
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                T* v = (T*)lua_touserdata(L, -1);
                CallMemberFunc<T, Args...>(v, FF);

                if constexpr (std::is_void_v<R>)
                    return 0;
                else
                    return 1;
            };
        };

        lua_pushstring(L, name);
        lua_pushcfunction(L, &Wrap::cfunc);
        lua_settable(L, -3);

        return *this;
    }

    template <class R, class... Args>
    LuaBinder<T>& BindMemFn(const char* name, R (T::*f)(Args...) const)
    {
        return BindMemFn(name, const_cast<R (T::*)(Args...)>(f));
    }

    template <class R, class... Args>
    LuaBinder<T>& BindStaticFn(const char* name, R (*f)(Args...))
    {
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                FF(ProcessArg<Args>()...);
                if constexpr (std::is_void_v<R>)
                    return 0;
                else
                    return 1;
            };
        };

        lua_pushstring(L, name);
        lua_pushcfunction(L, &Wrap::cfunc);
        lua_settable(L, -3);

        return *this;
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

        LuaBinder<GameScript> gameScript(L);
        gameScript.Begin("GameScript")
            .BindMemFn("Print", &GameScript::Print)
            .End();

        lua_setglobal(L, "wl");
    }
};
