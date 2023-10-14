#pragma once
#include "Core/Component/Transform.hpp"
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lua.hpp"
#include <functional>
#include <tuple>
#include <type_traits>

namespace Engine
{
#define LUA_WRAP_REGISTER_CLASS(ClassName)                                                                             \
    luaL_newmetatable(L, #ClassName);                                                                                  \
    lua_pushvalue(L, -1);                                                                                              \
    lua_setfield(L, -2, "__index");

#define LUA_WRAP_REGISTER_CFUNCTION(ClassName, Function)                                                               \
    lua_pushcfunction(L, Wrap<&ClassName::Function>(&ClassName::Function));                                            \
    lua_setfield(L, -2, #Function);

#define LUA_WRAP_REGISTER_CFUNCTION_OVERLOAD(ClassName, Function, Return, ...)                                         \
    lua_pushcfunction(                                                                                                 \
        L,                                                                                                             \
        (Wrap<static_cast<Return (ClassName::*)(__VA_ARGS__)>(&ClassName::Function), ClassName, Return, __VA_ARGS__>(  \
            &ClassName::Function                                                                                       \
        ))                                                                                                             \
    );                                                                                                                 \
    lua_setfield(L, -2, #Function);

#define ELSE_IF_IS_COMPONENT(Type)                                                                                     \
    else if constexpr (std::is_same_v<R, RefPtr<Type>>)                                                                \
    {                                                                                                                  \
        if (val != nullptr)                                                                                            \
        {                                                                                                              \
            *((Type**)lua_newuserdata(L, sizeof(void*))) = val.Get();                                                  \
            luaL_setmetatable(L, val->GetName().c_str());                                                              \
        }                                                                                                              \
    }

using LuaRef = int;
class LuaWraps
{
private:
    template <class... Args>
    static void WrapParam(lua_State* L, std::tuple<Args...>& args)
    {
        if constexpr (std::tuple_size_v < std::tuple < Args... >>> 0)
            WrapParamAtIndex<0, typename std::tuple_element<0, std::tuple<Args...>>::type, Args...>(L, args);
    }

    template <int index, class Type, class... Args>
    static void WrapParamAtIndex(lua_State* L, std::tuple<Args...>& args)
    {
        int paramIndex = index + 2;
        if constexpr (std::is_same_v<Type, glm::vec3>)
        {
            lua_rawgeti(L, paramIndex, 1);
            lua_rawgeti(L, paramIndex, 2);
            lua_rawgeti(L, paramIndex, 3);

            float x = luaL_checknumber(L, -3);
            float y = luaL_checknumber(L, -2);
            float z = luaL_checknumber(L, -1);

            std::get<index>(args) = glm::vec3(x, y, z);
        }
        else if constexpr (std::is_same_v<Type, const char*>)
        {
            const char* val = luaL_checkstring(L, paramIndex);

            std::get<index>(args) = val;
        }
        // extend parameter types here...
        // else static_assert(false);

        if constexpr (index + 1 < std::tuple_size_v<std::tuple<Args...>>)
            WrapParamAtIndex<index + 1, std::tuple_element<index + 1, std::tuple<Args...>>::type, Args...>(L, args);
    }

    template <class R, class T>
    static void WrapReturn(lua_State* L, T obj, R& val)
    {
        if constexpr (std::is_same_v<R, const glm::vec3&>)
        {
            lua_createtable(L, 3, 0);
            lua_pushnumber(L, val.x);
            lua_seti(L, -2, 1);
            lua_pushnumber(L, val.y);
            lua_seti(L, -2, 2);
            lua_pushnumber(L, val.z);
            lua_seti(L, -2, 3);
        }
        else if constexpr (std::is_same_v<std::remove_const_t<std::remove_reference_t<R>>, std::string>)
        {
            lua_pushstring(L, val.c_str());
        }
        ELSE_IF_IS_COMPONENT(Component)
        ELSE_IF_IS_COMPONENT(Transform)
        ELSE_IF_IS_COMPONENT(Scene)
        // extend return types here...
        // else static_assert(false);
    }

    template <auto ff, class T, class R, class... Args>
    auto Wrap(R (T::*notUsed)(Args...))
    {
        return [](lua_State* L) -> int
        {
            T** ptr = (T**)lua_touserdata(
                L,
                1
            ); // maybe we can wrap the name in a class' template arg which can make this lambda a c comptiable
               // function, so that we can test the data using luaL_checkudata
            if (ptr != nullptr)
            {
                std::tuple<typename std::remove_const<typename std::remove_reference<Args>::type>::type...> args;
                WrapParam<typename std::remove_const<typename std::remove_reference<Args>::type>::type...>(L, args);

                if constexpr (std::is_void<R>::value)
                {
                    std::apply(ff, std::tuple_cat(std::make_tuple(*ptr), args));
                    return 0;
                }
                else
                {
                    R val = std::apply(ff, std::tuple_cat(std::make_tuple(*ptr), args));
                    WrapReturn<R>(L, *ptr, val);
                    return 1;
                }
            }

            return 0;
        };
    }

    lua_State* L;

public:
    LuaWraps(lua_State* state) : L(state)
    {
        LUA_WRAP_REGISTER_CLASS(GameObject);
        //LUA_WRAP_REGISTER_CFUNCTION(GameObject, GetName);
        //LUA_WRAP_REGISTER_CFUNCTION(GameObject, GetTransform);
        // LUA_WRAP_REGISTER_CFUNCTION_OVERLOAD(GameObject, GetComponent, RefPtr<Component>, const char*);

        LUA_WRAP_REGISTER_CLASS(Transform);
        LUA_WRAP_REGISTER_CFUNCTION(Transform, SetPosition);
        LUA_WRAP_REGISTER_CFUNCTION(Transform, GetPosition);

        // register new lua binding here...
    }
};
} // namespace Engine
