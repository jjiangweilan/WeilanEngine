#pragma once
#include "Core/Component/GameScript.hpp"
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lauxlib.h"
#include "ThirdParty/lua/lua.hpp"
#include <typeindex>

class LuaTypeRegister
{
    template<class T>
    void PushTypeMetatable()
    {
        auto iter = typeToMetatableName.find(typeid(T));
        if (iter != typeToMetatableName.end())
        {
        }
    }

    std::unordered_map<std::type_index, std::string> typeToMetatableName;
};

template <class T>
class LuaBinder
{
public:
    LuaBinder(lua_State* L) { this->L = L; }

    LuaBinder<T>& Begin(const char* name)
    {
        this->name = name;

        luaL_newmetatable(L, name);

        lua_pushstring(L, "New");
        lua_pushcfunction(L, &LuaBinder<T>::New);
        lua_settable(L, -3);

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

                if constexpr (std::is_void_v<R>)
                {
                    CallMemberFunc<T, R, Args...>(L, v, FF);
                    return 0;
                }
                else
                {
                    R rtn = CallMemberFunc<T, R, Args...>(L, v, FF);
                    ProcessRtn<R>(L, std::move(rtn));

                    return 1;
                }
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
    static std::remove_reference_t<Type> ProcessArg(lua_State* L)
    {
        if constexpr (std::is_integral_v<Type>)
        {
            lua_Integer v = luaL_checkinteger(L, -1);
            return v;
        }
        else if constexpr (std::is_floating_point_v<Type>)
        {
            return luaL_checknumber(L, -1);
        }
        else
            return std::remove_reference_t<Type>{};
    }

    template <class R>
    static void ProcessRtn(lua_State* L, R&& v)
    {
        if constexpr (std::is_integral_v<R> && !std::is_same_v<R, bool>)
        {
            // Handle integers
            lua_pushinteger(L, static_cast<lua_Integer>(v));
        }
        else if constexpr (std::is_floating_point_v<R>)
        {
            // Handle floating-point types
            lua_pushnumber(L, static_cast<lua_Number>(v));
        }
        else if constexpr (std::is_same_v<R, const char*>)
        {
            // Handle const char*
            lua_pushstring(L, v);
        }
        else if constexpr (std::is_same_v<R, std::string>)
        {
            // Handle std::string
            lua_pushlstring(L, v.c_str(), v.size());
        }
        else if constexpr (std::is_same_v<R, bool>)
        {
            // Handle boolean types
            lua_pushboolean(L, v);
        }
        else
        {
            // Handle user-defined types
            lua_pushlightuserdata(L, static_cast<void*>(&v));

            R* m = lua_newuserdata(L, sizeof(R));
            *m = std::move(v);

            lua_pushstring(L, "_wl_runtimetype");
            lua_pushinteger(L, typeid(R).hash_code());
        }
    }

    /****** Type Processing *******/
    template <class ObjType, class R, class... Args>
    static auto CallMemberFunc(lua_State* L, ObjType* v, auto f)
    {
        if (std::is_void_v<R>)
            (v->*f)(ProcessArg<Args>(L)...);
        else
            return (v->*f)(ProcessArg<Args>(L)...);
    }

    static int New(lua_State* L)
    {
        // expecting a table on top of the stack
        lua_newtable(L);

        lua_pushstring(L, "__index");
        lua_pushvalue(L, -3);
        lua_settable(L, -4);

        lua_pushvalue(L, -2);
        lua_setmetatable(L, -2);

        return 1;
    }

    template <class R>
    void ReturnAsUserdata(R r)
    {}
};

class LuaBindings
{
public:
    // this function will leave a table on stack
    void BindClasses(lua_State* L)
    {
        lua_newtable(L);

        LuaBinder<GameScript> gameScript(L);
        gameScript.Begin("GameScript").BindMemFn("AddOne", &GameScript::AddOne).End();

        lua_setglobal(L, "wl");
    }
};
