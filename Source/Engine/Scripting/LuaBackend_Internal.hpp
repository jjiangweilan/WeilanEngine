#pragma once
#include "Core/Component/GameScript.hpp"
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lauxlib.h"
#include "ThirdParty/lua/lua.h"
#include "ThirdParty/lua/lua.hpp"
#include <typeindex>

struct LuaTypeRegistery
{
    static std::unordered_map<std::type_index, std::string> typeToName;
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
        LuaTypeRegistery::typeToName[typeid(T)] = name;

        lua_pushvalue(L, -1);
        lua_setfield(L, -1, "__index");

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

    template <class R, class TT, class... Args>
    LuaBinder<T>& BindMemFn(const char* name, R (TT::*f)(Args...))
    {
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                T* v = (T*)lua_touserdata(L, -1);

                if constexpr (std::is_void_v<R>)
                {
                    CallMemberFunc<R, Args...>(L, v, FF);
                    return 0;
                }
                else
                {
                    R rtn = CallMemberFunc<R, Args...>(L, v, FF);
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

    template <class R, class TT, class... Args>
    LuaBinder<T>& BindMemFn(const char* name, R (TT::*f)(Args...) const)
    {
        return BindMemFn(name, reinterpret_cast<R (TT::*)(Args...)>(f));
    }

    template <class R, class... Args>
    LuaBinder<T>& BindStaticFn(const char* name, R (*f)(Args...))
    {
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                FF(ProcessArg<Args>(L)...);
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
            void* m = lua_newuserdata(L, sizeof(R));
            new (m) R(std::move(v));

            // refactor to PushUserData()
            if constexpr (std::is_pointer_v<R>)
            {
                // TODO: push a special table to handle pointer
                PushTypeMetatable<std::remove_pointer_t<R>>(L);
            }
            else if constexpr (std::is_reference_v<R>)
            {
                // TODO: convert to pointer
                PushTypeMetatable<std::remove_reference_t<R>>(L);
            }
            else if constexpr (IsObjPtr<R>::value)
            {
                // TODO: push a special table to handle ObjPtr
                PushTypeMetatable<R::element_type>(L);
            }
            else // value type
            {
                PushTypeMetatable<R>(L);
            }

            lua_setmetatable(L, -2);
        }
    }

    template <class V>
    static void PushTypeMetatable(lua_State* L)
    {
        const char* name = nullptr;
        auto iter = LuaTypeRegistery::typeToName.find(typeid(V));
        if (iter != LuaTypeRegistery::typeToName.end())
        {
            name = iter->second.c_str();
        }
        else
            throw std::runtime_error("Type not registered");

        luaL_getmetatable(L, name);
    }

    using Lua_Ref = int;

    /****** Type Processing *******/
    template <class R, class... Args>
    static auto CallMemberFunc(lua_State* L, T* v, auto f)
    {
        if (std::is_void_v<R>)
            (v->*f)(ProcessArg<Args>(L)...);
        else
            return (v->*f)(ProcessArg<Args>(L)...);
    }

    static int New(lua_State* L)
    {
        // expecting a `self` table on top of the stack
        lua_newtable(L);

        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "__index");

        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "__newindex");

        lua_pushvalue(L, -2);
        lua_setmetatable(L, -2);

        return 1;
    }
};

class LuaBindings
{
public:
    // this function will leave a table on stack
    void BindClasses(lua_State* L)
    {
        lua_newtable(L);

        // clang-format off
        LuaBinder<GameScript> gameScript(L);
        gameScript
            .Begin("GameScript")
            .BindMemFn("AddOne", &GameScript::AddOne)
            .BindMemFn("GetGameObject", &GameScript::GetGameObject)
            .End();

        LuaBinder<GameObject> gameObject(L);
        gameObject
            .Begin("GameObject")
            .BindMemFn("GetPosition", &GameObject::GetPosition)
            .End();

        LuaBinder<glm::vec3> vec3(L);
        vec3
            .Begin("Vec")
            .BindStaticFn("Dot", &glm::dot<3, float, glm::packed_highp>)
            .End();

        // clang-format on

        lua_setglobal(L, "wl");
    }
};
