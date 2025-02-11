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

// decay value and ObjPtr to raw pointer
struct UserDataDecay
{
    static int ValueDecay(lua_State* L) { return 1; }
};

enum class LuaEngineUserDataType
{
    Value,
    RawPtr,
    ObjPtr
};

struct LuaEngineTableField
{
    inline static const char* dataType = "__wl_dataType";
};

struct PushEngineUserDataHelper
{
    template <class R>
    static void Execute(lua_State* L, R&& v)
    {
        // Handle user-defined types
        void* m = lua_newuserdata(L, sizeof(R));

        // refactor to PushUserData()
        lua_newtable(L);
        if constexpr (std::is_pointer_v<R>)
        {
            new (m) R(std::move(v));
            lua_pushinteger(L, (int)LuaEngineUserDataType::RawPtr);
            lua_setfield(L, -2, LuaEngineTableField::dataType);

            PushTypeMetatable<std::remove_pointer_t<R>>(L);
            lua_setmetatable(L, -2);
        }
        else if constexpr (std::is_reference_v<R>)
        {
            new (m) R(std::move(&v));
            lua_pushinteger(L, (int)LuaEngineUserDataType::RawPtr);
            lua_setfield(L, -2, LuaEngineTableField::dataType);

            PushTypeMetatable<std::remove_reference_t<R>>(L);
            lua_setmetatable(L, -2);
        }
        else if constexpr (IsObjPtr<R>::value)
        {
            new (m) R(std::move(v));
            // TODO: push a special table to handle ObjPtr
            lua_pushinteger(L, (int)LuaEngineUserDataType::ObjPtr);
            lua_setfield(L, -2, LuaEngineTableField::dataType);

            PushTypeMetatable<R::element_type>(L);
            lua_setmetatable(L, -2);
        }
        else // value type
        {
            new (m) R(std::move(v));
            // TODO: push a special table to handle ObjPtr
            lua_pushinteger(L, (int)LuaEngineUserDataType::Value);
            lua_setfield(L, -2, LuaEngineTableField::dataType);

            PushTypeMetatable<R>(L);
            lua_setmetatable(L, -2);
        }
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_setmetatable(L, -2);
    }

private:
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
        // TODO: use c closure instead
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                void* u = (void*)lua_touserdata(L, 1);
                lua_getfield(L, 1, LuaEngineTableField::dataType);
                LuaEngineUserDataType type = (LuaEngineUserDataType)lua_tointeger(L, -1);
                lua_pop(L, 1);

                if (type == LuaEngineUserDataType::RawPtr)
                {
                    T* v = *(T**)u;
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
                }
                else if (type == LuaEngineUserDataType::ObjPtr)
                {
                    if constexpr (std::is_base_of_v<Object, T>)
                    {
                        if constexpr (std::is_void_v<R>)
                        {
                            T* v = *(ObjPtr<T>*)u;
                            CallMemberFunc<R, Args...>(L, v, FF);
                            return 0;
                        }
                        else
                        {
                            T* v = *(ObjPtr<T>*)u;
                            R rtn = CallMemberFunc<R, Args...>(L, v, FF);
                            ProcessRtn<R>(L, std::move(rtn));

                            return 1;
                        }
                    }
                    else
                        return 0;
                }
                else // LuaEngineUserDataType::Value
                {
                    T* v = (T*)u;
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

    template <class R, class TT, class... Args>
    LuaBinder<T>& BindMemFn(const char* name, const R (TT::*f)(Args...) const)
    {
        return BindMemFn(name, reinterpret_cast<R (TT::*)(Args...)>(f));
    }

    template <class R, class... Args>
    LuaBinder<T>& BindStaticFn(const char* name, R (*f)(Args...))
    {
        // TODO: use c closure instead
        static auto FF = f;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                ProcessArg_StaticFunction<std::tuple<Args...>, R>(
                    L,
                    FF,
                    0,
                    std::make_index_sequence<sizeof...(Args)>{}
                );
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

    template <class F>
    LuaBinder<T>& BindFn(const char* name, F&& f)
    {
        return BindFn(name, std::function(std::move(f)));
    }

private:
    template <class R, class... Args>
    LuaBinder<T>& BindFn(const char* name, std::function<R(T& val, Args...)>&& f)
    {
        using FT = std::function<R(T & val, Args...)>;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                int fIndex = lua_upvalueindex(1);
                FT& f = *(FT*)lua_touserdata(L, fIndex);

                void* u = (void*)lua_touserdata(L, 1);
                lua_getfield(L, 1, LuaEngineTableField::dataType);
                LuaEngineUserDataType type = (LuaEngineUserDataType)lua_tointeger(L, -1);
                lua_pop(L, 1);

                if (type == LuaEngineUserDataType::RawPtr)
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        T* v = *(T**)u;
                        ProcessArg<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
                        return 0;
                    }
                    else
                    {
                        T* v = *(T**)u;
                        R rtn =
                            ProcessArg<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
                        ProcessRtn<R>(L, std::move(rtn));

                        return 1;
                    }
                }
                else if (type == LuaEngineUserDataType::ObjPtr)
                {
                    if constexpr (std::is_base_of_v<Object, T>)
                    {
                        if constexpr (std::is_void_v<R>)
                        {
                            T* v = *(ObjPtr<T>*)u;
                            ProcessArg<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
                            return 0;
                        }
                        else
                        {
                            T* v = *(ObjPtr<T>*)u;
                            R rtn = ProcessArg<std::tuple<Args...>, R>(
                                L,
                                f,
                                v,
                                1,
                                std::make_index_sequence<sizeof...(Args)>{}
                            );
                            ProcessRtn<R>(L, std::move(rtn));

                            return 1;
                        }
                    }
                    else
                        return 0;
                }
                else // LuaEngineUserDataType::Value
                {
                    if constexpr (std::is_void_v<R>)
                    {
                        T* v = (T*)u;
                        ProcessArg<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
                        return 0;
                    }
                    else
                    {
                        T* v = (T*)u;
                        R rtn =
                            ProcessArg<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
                        ProcessRtn<R>(L, std::move(rtn));

                        return 1;
                    }
                }
            };
        };

        lua_pushstring(L, name);
        void* fm = lua_newuserdata(L, sizeof(FT)); // f as upvalue
        new (fm) FT(std::move(f));
        lua_pushcclosure(L, &Wrap::cfunc, 1);
        lua_settable(L, -3);
        return *this;
    }

    lua_State* L;
    const char* name;

    template <class Tuple, class R, size_t... I>
    static R ProcessArg(lua_State* L, auto& f, T* v, int argOffset, std::index_sequence<I...>)
    {
        return f(*v, ProcessArgImpl<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    template <class Tuple, class R, size_t... I>
    static R ProcessArg_FunctionPointer(lua_State* L, auto& f, T* v, int argOffset, std::index_sequence<I...>)
    {
        return (v->*f)(ProcessArgImpl<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    template <class Tuple, class R, size_t... I>
    static R ProcessArg_StaticFunction(lua_State* L, auto& f, int argOffset, std::index_sequence<I...>)
    {
        return f(ProcessArgImpl<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    template <class Type>
    static std::remove_reference_t<Type> ProcessArgImpl(lua_State* L, int argOffset, size_t idx)
    {
        if constexpr (std::is_integral_v<Type>)
        {
            lua_Integer v = luaL_checkinteger(L, argOffset + idx + 1);
            return v;
        }
        else if constexpr (std::is_floating_point_v<Type>)
        {
            lua_Number v = luaL_checknumber(L, argOffset + idx + 1);
            return v;
        }
        else if constexpr (std::is_same_v<Type, const char*>)
		{
			const char* v = luaL_checkstring(L, argOffset + idx + 1);
			return v;
		}
		else if constexpr (std::is_same_v<Type, std::string>)
		{
			size_t len;
			const char* v = luaL_checklstring(L, argOffset + idx + 1, &len);
			return std::string(v, len);
		}
		else if constexpr (std::is_same_v<Type, bool>)
		{
			bool v = lua_toboolean(L, argOffset + idx + 1);
			return v;
		}
		else
		{
			void* u = lua_touserdata(L, argOffset + idx + 1);
			lua_getfield(L, argOffset + idx + 1, LuaEngineTableField::dataType);
			LuaEngineUserDataType type = (LuaEngineUserDataType)lua_tointeger(L, -1);
			lua_pop(L, 1);

            using RawType = std::remove_const_t<std::remove_reference_t<Type>>;
			if (type == LuaEngineUserDataType::RawPtr)
			{
                return **(RawType**)(u);
			}
			else if (type == LuaEngineUserDataType::ObjPtr)
			{
                if constexpr (std::is_base_of_v<Object, Type>)
                {
                    return *(ObjPtr<Type>*)u;
                }
			}
			else // LuaEngineUserDataType::Value
			{
				return *(RawType*)u;
			}
		}
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
            PushEngineUserDataHelper::Execute(L, std::move(v));
        }
    }

    using Lua_Ref = int;

    /****** Type Processing *******/
    template <class R, class... Args>
    static auto CallMemberFunc(lua_State* L, T* v, auto f)
    {
        if (std::is_void_v<R>)
            ProcessArg_FunctionPointer<std::tuple<Args...>, R>(L, f, v, 1, std::make_index_sequence<sizeof...(Args)>{});
        else
            return ProcessArg_FunctionPointer<std::tuple<Args...>, R>(
                L,
                f,
                v,
                1,
                std::make_index_sequence<sizeof...(Args)>{}
            );
    }

    static int New(lua_State* L)
    {
        // expecting a `self` table on top of the stack
        lua_newtable(L);

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

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
            .BindMemFn("SetPosition", &GameObject::SetPosition)
            .End();

        LuaBinder<glm::vec3> vec3(L);
        vec3
            .Begin("Vec")
            .BindFn("GetX", [](glm::vec3& val) {return val[0]; })
            .BindFn("GetY", [](glm::vec3& val) {return val[1]; })
            .BindFn("GetZ", [](glm::vec3& val) {return val[2]; })
            .BindFn("SetX", [](glm::vec3& val, float v) {val[0] = v; })
            .BindFn("SetY", [](glm::vec3& val, float v) {val[1] = v; })
            .BindFn("SetZ", [](glm::vec3& val, float v) {val[2] = v; })
            .BindStaticFn("Dot", &glm::dot<3, float, glm::packed_highp>)
            .End();

        // clang-format on

        lua_setglobal(L, "wl");
    }
};
