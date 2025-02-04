#pragma once
#include "Core/GameObject.hpp"
#include "ThirdParty/lua/lua.hpp"

namespace LuaBindingProcessor
{

/****** Type Processing *******/
template <class Type>
static std::remove_reference_t<Type> ProcessArg()
{
    return Type{};
}

/****** Type Processing Dispatchers *******/
template <class... Args>
struct ProcessArgs;

template <class Type, class... Rest>
struct ProcessArgs<Type, Rest...>
{
    static auto Dispatch()
    {
        std::tuple<Type> arg = ProcessArg<Type>();
        auto rest = ProcessArgs<Rest...>::Dispatch();

        return std::tuple_cat(std::move(arg), std::move(rest));

    }
};

template <>
struct ProcessArgs<>
{
    static std::tuple<> Dispatch() { return std::make_tuple(); }
};

/****************************************/
} // namespace LuaBindingProcessor

template <class T>
class LuaBinder
{
public:
    template <class R, class... Args>
    static void Bind(const char* name, R (T::*f)(Args...))
    {
        LuaBindingProcessor::ProcessArgs<Args...>::Dispatch();
    }

    template <class R, class... Args>
    static void Bind(const char* name, R (T::*f)(Args...) const)
    {
        Bind(name, reinterpret_cast<R (T::*)(Args...)>(f));
    }
};

class LuaScript_LuaBinding
{
public:
    void BindClass(lua_State* L)
    {
        luaL_newmetatable(L, "GameScript");
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2); /* pushes the metatable */
        lua_settable(L, -3);  /* metatable.__index = metatable */

        const luaL_Reg funcs[] = {{"New", New}, {"GetComponent", GetComponent}, {nullptr, nullptr}};
        luaL_setfuncs(L, funcs, 0);
    }

    static int New(lua_State* L)
    {
        lua_newtable(L);
        luaL_setmetatable(L, "GameScript");

        return 1;
    }

    static int GetComponent(lua_State* L) { return 0; }
};

class GameObject_LuaBinding
{
public:
    void Bind(GameObject* self, lua_State* L)
    {
        GameObject** ptr = (GameObject**)lua_newuserdata(L, sizeof(void*));
        *ptr = self;

        LuaBinder<GameObject>::Bind("GetPosition", &GameObject::GetPosition);
        LuaBinder<GameObject>::Bind("GetPosition", &GameObject::SetPosition);
    }

private:
};
