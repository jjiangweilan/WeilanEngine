#pragma once
#include "Core/Component/AnimationPlayer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/GameScript.hpp"
#include "Core/Component/Light.hpp"
#include "Core/GameObject.hpp"
#include "Core/Object.hpp"
#include "GamePlay/Input.hpp"
#include "Libs/Serialization/Serializable.hpp"
#include "ThirdParty/lua/lauxlib.h"
#include "ThirdParty/lua/lua.h"
#include "ThirdParty/lua/lua.hpp"
#include <stdexcept>
#include <typeindex>

struct LuaTypeRegistery
{
    static std::unordered_map<std::type_index, std::string> typeToName;
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
    inline static const char* propertiesGet = "__wl_properties_get";
    inline static const char* propertiesSet = "__wl_properties_set";
    inline static const char* className = "__wl_className";
};

template <class T>
struct LuaUserDataPack
{
    LuaEngineUserDataType dataType;
    T val;

    // expectedClassName: class name of val
    void Assign(const char* expectedClassName, ObjPtr<Object> assigningObject)
    {
        if constexpr (IsObject<T>)
        {
            if (assigningObject == nullptr || strcmp(expectedClassName, assigningObject->GetTypeName().c_str()) == 0)
            {
                this->val = assigningObject;
            }
            else
            {
                spdlog::warn(
                    "Lua: type mismatch, expected: {}, got: {}",
                    expectedClassName,
                    assigningObject->GetTypeName()
                );
            }
        }
    }
};

template <class T>
T* GetLuaUserDataPackValue(lua_State* L, int idx)
{
    void* mem = lua_touserdata(L, 1);
    LuaEngineUserDataType type = *(LuaEngineUserDataType*)mem;

    if (type == LuaEngineUserDataType::RawPtr)
    {
        return ((LuaUserDataPack<T*>*)mem)->val;
    }
    else if (type == LuaEngineUserDataType::ObjPtr)
    {
        if constexpr (std::is_base_of_v<Object, T>)
            return &*(((LuaUserDataPack<ObjPtr<T>>*)mem)->val);
        else
            return nullptr;
    }
    else // LuaEngineUserDataType::Value
    {
        return &(((LuaUserDataPack<T>*)mem)->val);
    }
}

struct InvalidObjPtrError : std::runtime_error
{
    explicit InvalidObjPtrError(const char* _Message) : std::runtime_error(_Message) {}
};

template <class R>
struct PushEngineUserDataHelper
{
    // push a userdata of value v with a per userdata metatable
    static void Execute(lua_State* L, R&& v)
    {
        // Handle user-defined types
        void* m = lua_newuserdata(L, sizeof(LuaUserDataPack<R>));

        using RawType = std::remove_const_t<std::remove_reference_t<R>>;
        // refactor to PushUserData()
        lua_newtable(L);
        if constexpr (std::is_pointer_v<RawType>)
        {
            new (m) LuaUserDataPack<R>(LuaEngineUserDataType::RawPtr, std::move(v));
            PushTypeMetatable<std::remove_pointer_t<R>>(L);
        }
        else if constexpr (std::is_reference_v<RawType>)
        {
            new (m) LuaUserDataPack<R>(LuaEngineUserDataType::RawPtr, std::move(&v));
            PushTypeMetatable<std::remove_reference_t<R>>(L);
        }
        else if constexpr (IsObjPtr<RawType>::value)
        {
            new (m) LuaUserDataPack<R>(LuaEngineUserDataType::ObjPtr, std::move(v));
            PushTypeMetatable(L, v->GetTypeName().c_str());
        }
        else // value type
        {
            new (m) LuaUserDataPack<R>(LuaEngineUserDataType::Value, std::move(v));
            PushTypeMetatable<R>(L);
        }
        lua_setmetatable(L, -2); // set userdat metatable's metatable
        lua_pushcfunction(L, &Index);
        lua_setfield(L, -2, "__index");

        lua_pushcfunction(L, &NewIndex);
        lua_setfield(L, -2, "__newindex");

        lua_setmetatable(L, -2);
    }

    static int Index(lua_State* L)
    {
        ASSERT(lua_isuserdata(L, 1));         // 1
        const char* key = lua_tostring(L, 2); // 2

        int currentInspectingTable = 3;
        lua_getmetatable(L, 1); // 3
        int oldTop = lua_gettop(L);
        do // currentInspectingTable + 1
        {
            lua_pushstring(L, LuaEngineTableField::propertiesGet);
            if (lua_rawget(L, -2) == LUA_TTABLE) // + 2
            {
                lua_pushvalue(L, 2);                    // push the key
                if (lua_rawget(L, -2) == LUA_TFUNCTION) // + 3
                {
                    lua_pushvalue(L, 1); // 6

                    if (lua_pcall(L, 1, 1, 0)) // pop 6, +3
                    {
                        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                    }

                    return 1;
                }
                else
                {
                    lua_pop(L, 2); // pop + 2/3
                }
            }
        }
        while (lua_getmetatable(L, currentInspectingTable++));
        // property not found, continue ...
        int newTop = lua_gettop(L);
        lua_pop(L, newTop - oldTop); // cleanup

        lua_getfield(L, 3, key);
        return 1;
    }

    static int NewIndex(lua_State* L)
    {
        ASSERT(lua_isuserdata(L, 1));         // 1
        const char* key = lua_tostring(L, 2); // 2

        int currentInspectingTable = 4;
        lua_getmetatable(L, 1); // 4
        int oldTop = lua_gettop(L);
        do // currentInspectingTable + 1
        {
            lua_pushstring(L, LuaEngineTableField::propertiesSet);
            if (lua_rawget(L, -2) == LUA_TTABLE) // + 2
            {
                lua_pushvalue(L, 2);                    // push the key
                if (lua_rawget(L, -2) == LUA_TFUNCTION) // + 3
                {
                    lua_pushvalue(L, 1); // 6
                    lua_pushvalue(L, 3); // 7

                    if (lua_pcall(L, 2, 1, 0)) // pop 6,7,+3
                    {
                        SPDLOG_ERROR("Lua Error: {}", lua_tostring(L, -1));
                    }

                    return 1;
                }
                else
                {
                    lua_pop(L, 2); // pop + 2/3
                }
            }
        }
        while (lua_getmetatable(L, currentInspectingTable++));
        // property not found, continue ...
        int newTop = lua_gettop(L);
        lua_pop(L, newTop - oldTop); // cleanup

        lua_getfield(L, 4, key);
        return 1;
    }

private:
    static void PushTypeMetatable(lua_State* L, const char* className)
    {
        luaL_getmetatable(L, className);
        ASSERT(lua_istable(L, -1));
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
        lua_pushcclosure(L, &Index, 1);
        lua_setfield(L, -2, "__index");

        // push serialization
        if constexpr (IsSerializable<T>)
        {
            BindMemFn("Serialize", &T::Serialize);
            BindMemFn("Deserialize", &T::Deserialize);
        }

        if constexpr (CanBeSerializerParameter<T>)
        {
            BindFn("SerializeTo", [](T& val, const char* name, Serializer* s) { s->Serialize(name, val); });
            BindFn("DeserializeTo", [](T& val, const char* name, Serializer* s) { s->Deserialize(name, val); });
        }

        // register container type
        // Vector
        // ObjPtr

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
        using FT = decltype(f);
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                int fIndex = lua_upvalueindex(1);
                FT& f = *(FT*)lua_touserdata(L, fIndex);

                void* mem = lua_touserdata(L, 1);
                LuaEngineUserDataType type = *(LuaEngineUserDataType*)mem;

                using RawType = std::remove_const_t<std::remove_reference_t<R>>;
                if (type == LuaEngineUserDataType::RawPtr)
                {
                    T* v = ((LuaUserDataPack<T*>*)mem)->val;
                    if constexpr (std::is_void_v<RawType>)
                    {
                        CallMemberFunc<R, Args...>(L, v, f);
                        return 0;
                    }
                    else
                    {
                        R rtn = CallMemberFunc<R, Args...>(L, v, f);
                        ProcessRtn<R>(L, std::move(rtn));

                        return 1;
                    }
                }
                else if (type == LuaEngineUserDataType::ObjPtr)
                {
                    if constexpr (std::is_base_of_v<Object, T>)
                    {
                        T* v = &*(((LuaUserDataPack<ObjPtr<T>>*)mem)->val);
                        if constexpr (std::is_void_v<RawType>)
                        {
                            CallMemberFunc<R, Args...>(L, v, f);
                            return 0;
                        }
                        else
                        {
                            R rtn = CallMemberFunc<R, Args...>(L, v, f);
                            ProcessRtn<R>(L, std::move(rtn));

                            return 1;
                        }
                    }
                    else
                        return 0;
                }
                else // LuaEngineUserDataType::Value
                {
                    T* v = &(((LuaUserDataPack<T>*)mem)->val);
                    if constexpr (std::is_void_v<RawType>)
                    {
                        CallMemberFunc<R, Args...>(L, v, f);
                        return 0;
                    }
                    else
                    {
                        R rtn = CallMemberFunc<R, Args...>(L, v, f);
                        ProcessRtn<R>(L, std::move(rtn));

                        return 1;
                    }
                }
            };
        };

        lua_pushstring(L, name);
        void* fm = lua_newuserdata(L, sizeof(FT)); // f as upvalue
        new (fm) FT(f);
        lua_pushcclosure(L, &Wrap::cfunc, 1);
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
    LuaBinder<T>& BindStaticFn(const char* name, std::function<R(Args...)>&& f)
    {
        using FT = std::function<R(Args...)>;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                int fIndex = lua_upvalueindex(1);
                FT& f = *(FT*)lua_touserdata(L, fIndex);

                if constexpr (std::is_void_v<R>)
                {
                    CallbackDispatch_StaticFunction<std::tuple<Args...>, R>(
                        L,
                        f,
                        0,
                        std::make_index_sequence<sizeof...(Args)>{}
                    );
                    return 0;
                }
                else if constexpr (std::tuple_size_v<std::tuple<Args...>> == 1)
                {
                    if constexpr (std::is_same_v<std::tuple_element_t<0, std::tuple<Args...>>, lua_State*> &&
                                  std::is_integral_v<R>)
                    {
                        return f(L);
                    }
                }
                // else
                {
                    R rtn = CallbackDispatch_StaticFunction<std::tuple<Args...>, R>(
                        L,
                        f,
                        0,
                        std::make_index_sequence<sizeof...(Args)>{}
                    );
                    ProcessRtn<R>(L, std::move(rtn));
                    return 1;
                }
            };
        };

        lua_pushstring(L, name);
        void* fm = lua_newuserdata(L, sizeof(FT)); // f as upvalue
        new (fm) FT(f);
        lua_pushcclosure(L, &Wrap::cfunc, 1);
        lua_settable(L, -3);

        return *this;
    }

    template <class F>
    LuaBinder<T>& BindStaticFn(const char* name, F&& f)
    {
        return BindStaticFn(name, std::function(std::move(f)));
    }

    template <class R, class... Args>
    LuaBinder<T>& BindStaticFn(const char* name, R (*f)(Args...))
    {
        return BindStaticFn(name, std::function<R(Args...)>([f](Args... args) -> R { return f(args...); }));
    }

    template <class F>
    LuaBinder<T>& BindFn(const char* name, F&& f)
    {
        return BindFn(name, std::function(std::move(f)));
    }

    template <class V>
    LuaBinder<T>& BindProperty(const char* name, V T::* p)
    {
        return BindProperty(name, [p](T& val) { return val.*p; }, [p](T& val, const V& v) { val.*p = v; });
    }

    template <class Getter, class Setter>
    LuaBinder<T>& BindProperty(const char* name, Getter&& getter, Setter&& setter)
    {
        if (!hasPropertyTable)
        {
            lua_pushstring(L, LuaEngineTableField::propertiesGet);
            lua_newtable(L);
            lua_settable(L, -3);

            lua_pushstring(L, LuaEngineTableField::propertiesSet);
            lua_newtable(L);
            lua_settable(L, -3);
            hasPropertyTable = true;
        }

        lua_getfield(L, -1, LuaEngineTableField::propertiesGet); // 3
        BindFn(name, std::move(getter));
        lua_pop(L, 1);

        lua_getfield(L, -1, LuaEngineTableField::propertiesSet); // 4
        BindFn(name, std::move(setter));
        lua_pop(L, 1);
        return *this;
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
            PushEngineUserDataHelper<R>::Execute(L, std::move(v));
        }
    }

    template <class Type>
    static auto ProcessArg(lua_State* L, int argOffset, size_t idx)
    {
        // TODO: we need to determine what we can actually return here for the case where the input is a pointer.
        // It can be a raw pointer, an ObjPtr, or a value(by deference). a logic should be determined here.
        using RawType = std::remove_const_t<std::remove_reference_t<Type>>;
        if constexpr (std::is_integral_v<RawType>)
        {
            lua_Integer v = luaL_checkinteger(L, argOffset + idx + 1);
            return v;
        }
        else if constexpr (std::is_floating_point_v<RawType>)
        {
            lua_Number v = luaL_checknumber(L, argOffset + idx + 1);
            return v;
        }
        else if constexpr (std::is_same_v<RawType, const char*>)
        {
            const char* v = luaL_checkstring(L, argOffset + idx + 1);
            return v;
        }
        else if constexpr (std::is_same_v<RawType, std::string>)
        {
            size_t len;
            const char* v = luaL_checklstring(L, argOffset + idx + 1, &len);
            return std::string(v, len);
        }
        else if constexpr (std::is_same_v<RawType, bool>)
        {
            bool v = lua_toboolean(L, argOffset + idx + 1);
            return v;
        }
        else
        {
            ASSERT(lua_isuserdata(L, argOffset + idx + 1));
            void* mem = lua_touserdata(L, argOffset + idx + 1);
            LuaEngineUserDataType type = *(LuaEngineUserDataType*)mem;

            using RawType = std::remove_const_t<std::remove_reference_t<Type>>;
            if (type == LuaEngineUserDataType::RawPtr)
            {
                return *((LuaUserDataPack<RawType*>*)mem)->val;
            }
            else if (type == LuaEngineUserDataType::ObjPtr)
            {
                if constexpr (std::is_base_of_v<Object, Type>)
                {
                    ObjPtr<Object> obj = ((LuaUserDataPack<ObjPtr<Object>>*)mem)->val;
                    if (obj == nullptr || obj->GetObjectTypeID() != Type::StaticGetObjectTypeID())
                    {
                        luaL_error(L, "Invalid object type");
                        throw InvalidObjPtrError("Valida Object Type");
                    }
                    return *obj;
                }
                // now falling back to Value type, this can happen in the following case
                // T::f(const GameObject& go) <- lua: go:f(self.A_objPtr), calling above
                // T::f(ObjPtr<GameObject> go) <- lua: go:f(self.A_objPtr), this will fall back to Value type which
                // ObjPtr<GameObject> is not based of Object

                // before falling back the Value type, we still need to do a type check
                if constexpr (IsObjPtr<RawType>::value)
                {
                    ObjPtr<Object> obj = ((LuaUserDataPack<ObjPtr<Object>>*)mem)->val;
                    if constexpr (!std::is_same_v<typename RawType::element_type, Object>)
                    {
                        if (obj == nullptr || obj->GetObjectTypeID() != RawType::element_type::StaticGetObjectTypeID())
                        {
                            luaL_error(L, "Invalid object type");
                            throw InvalidObjPtrError("Valida Object Type");
                        }
                    }
                }
            }

            return ((LuaUserDataPack<RawType>*)mem)->val; // LuaEngineUserDataType::Value
        }
    }

private:
    bool hasPropertyTable = false;
    lua_State* L = nullptr;
    const char* name = "";

    static int Index(lua_State* L)
    {
        const char* key = lua_tostring(L, 2);
        int tableIdx = lua_upvalueindex(1);
        lua_getfield(L, tableIdx, key);

        return 1;
    }

    LuaBinder<T>& BindFn(const char* name, std::function<int(lua_State*)>&& f)
    {
        using FT = std::function<int(lua_State*)>;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                int fIndex = lua_upvalueindex(1);
                FT& f = *(FT*)lua_touserdata(L, fIndex);
                ASSERT(lua_istable(L, 1));

                return f(L);
            }
        };

        lua_pushstring(L, name);
        void* fm = lua_newuserdata(L, sizeof(FT)); // f as upvalue
        new (fm) FT(std::move(f));
        lua_pushcclosure(L, &Wrap::cfunc, 1);
        lua_settable(L, -3);
        return *this;
    }

    template <class R, class... Args>
    LuaBinder<T>& BindFn(const char* name, std::function<R(T& val, Args...)>&& f)
    {
        using FT = std::function<R(T & val, Args...)>;
        struct Wrap
        {
            static int cfunc(lua_State* L)
            {
                try
                {
                    int fIndex = lua_upvalueindex(1);
                    FT& f = *(FT*)lua_touserdata(L, fIndex);

                    void* u = lua_touserdata(L, 1);
                    LuaEngineUserDataType type = *(LuaEngineUserDataType*)u;

                    using RawType = std::remove_const_t<std::remove_reference_t<R>>;
                    if (type == LuaEngineUserDataType::RawPtr)
                    {
                        if constexpr (std::is_void_v<RawType>)
                        {
                            T* v = ((LuaUserDataPack<T*>*)u)->val;
                            CallbackDispatch<std::tuple<Args...>, R>(
                                L,
                                f,
                                v,
                                1,
                                std::make_index_sequence<sizeof...(Args)>{}
                            );
                            return 0;
                        }
                        else
                        {
                            T* v = ((LuaUserDataPack<T*>*)u)->val;
                            R rtn = CallbackDispatch<std::tuple<Args...>, R>(
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
                    else if (type == LuaEngineUserDataType::ObjPtr)
                    {
                        if constexpr (std::is_base_of_v<Object, T>)
                        {
                            if constexpr (std::is_void_v<RawType>)
                            {
                                T* v = &*(((LuaUserDataPack<ObjPtr<T>>*)u)->val);
                                CallbackDispatch<std::tuple<Args...>, R>(
                                    L,
                                    f,
                                    v,
                                    1,
                                    std::make_index_sequence<sizeof...(Args)>{}
                                );
                                return 0;
                            }
                            else
                            {
                                T* v = &*(((LuaUserDataPack<ObjPtr<T>>*)u)->val);
                                R rtn = CallbackDispatch<std::tuple<Args...>, R>(
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
                        if constexpr (std::is_void_v<RawType>)
                        {
                            T* v = &(((LuaUserDataPack<T>*)u)->val);
                            CallbackDispatch<std::tuple<Args...>, R>(
                                L,
                                f,
                                v,
                                1,
                                std::make_index_sequence<sizeof...(Args)>{}
                            );
                            return 0;
                        }
                        else
                        {
                            T* v = &(((LuaUserDataPack<T>*)u)->val);
                            R rtn = CallbackDispatch<std::tuple<Args...>, R>(
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
                }
                catch (InvalidObjPtrError e)
                {
                    spdlog::error("Lua Error: {}", e.what());
                    return 0;
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

    template <class Tuple, class R, size_t... I>
    static R CallbackDispatch(lua_State* L, auto& f, T* v, int argOffset, std::index_sequence<I...>)
    {
        return f(*v, ProcessArg<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    template <class Tuple, class R, size_t... I>
    static R CallbackDispatch_FunctionPointer(lua_State* L, auto& f, T* v, int argOffset, std::index_sequence<I...>)
    {
        return (v->*f)(ProcessArg<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    template <class Tuple, class R, size_t... I>
    static R CallbackDispatch_StaticFunction(lua_State* L, auto& f, int argOffset, std::index_sequence<I...>)
    {
        return f(ProcessArg<std::tuple_element_t<I, Tuple>>(L, argOffset, I)...);
    }

    using Lua_Ref = int;

    /****** Type Processing *******/
    template <class R, class... Args>
    static auto CallMemberFunc(lua_State* L, T* v, auto f)
    {
        if (std::is_void_v<R>)
            CallbackDispatch_FunctionPointer<std::tuple<Args...>, R>(
                L,
                f,
                v,
                1,
                std::make_index_sequence<sizeof...(Args)>{}
            );
        else
            return CallbackDispatch_FunctionPointer<std::tuple<Args...>, R>(
                L,
                f,
                v,
                1,
                std::make_index_sequence<sizeof...(Args)>{}
            );
    }

    static int New(lua_State* L) {}
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
            .BindFn("New", [](lua_State* L){
                    // expecting a `self` table on top of the stack
                    ASSERT(lua_istable(L, 1));
                    ASSERT(lua_isstring(L, 2));
                    const char* className = lua_tostring(L, 2);

                    lua_newtable(L);

                    lua_pushvalue(L, -1);
                    lua_setfield(L, -2, "__index");

                    lua_pushvalue(L, 1);
                    lua_setmetatable(L, -2);

                    lua_pushstring(L, className);
                    lua_setfield(L, 3, LuaEngineTableField::className);

                    return 1;
                    })
            .BindMemFn("GetGameObject", &GameScript::GetGameObject)
            .End();

        LuaBinder<GameObject> gameObject(L);
        gameObject
            .Begin("GameObject")
            .BindMemFn("GetPosition", &GameObject::GetPosition)
            .BindMemFn("SetPosition", &GameObject::SetPosition)
            .BindMemFn("GetComponentInHierachy", &GameObject::GetComponentInHierachy)
            .BindMemFn("LookAt", &GameObject::LookAt)
            .BindFn("GetComponent", [](lua_State* L) -> int {
                    GameObject* go = GetLuaUserDataPackValue<GameObject>(L, 1);
                    const char* className = luaL_checkstring(L, 2);

                    if (go == nullptr || className == nullptr)
                        return 0;
                    
                    auto v = go->GetComponent(className);
                    if (v != nullptr)
                    {
                        LuaBinder<GameObject>::ProcessRtn(L, std::move(v));
                        return 1;
                    }

                    // failed to get Engine Component, try Lua Script
                    auto gameScript = go->GetComponent<GameScript>();
                    if(gameScript != nullptr)
                    {
                        auto& luaClassName = gameScript->GetLuaClassName();
                        if (luaClassName == className)
                        {
                            return gameScript->LuaPushReferenceToStack();
                        }
                    }

                    return 0;
                })
            .End();

        LuaBinder<glm::vec3> vec3(L);
        vec3
            .Begin("Vec3")
            .BindStaticFn("New", [](){ return glm::vec3{0,0,0}; })
            .BindProperty("x", &glm::vec3::x)
            .BindProperty("y", &glm::vec3::y)
            .BindProperty("z", &glm::vec3::z)
            .BindFn("GetX", [](glm::vec3& val) {return val[0]; })
            .BindFn("GetY", [](glm::vec3& val) {return val[1]; })
            .BindFn("GetZ", [](glm::vec3& val) {return val[2]; })
            .BindFn("SetX", [](glm::vec3& val, float v) {val[0] = v; })
            .BindFn("SetY", [](glm::vec3& val, float v) {val[1] = v; })
            .BindFn("SetZ", [](glm::vec3& val, float v) {val[2] = v; })
            .BindStaticFn("Dot", &glm::dot<3, float, glm::packed_highp>)
            .End();

        LuaBinder<Input> input(L);
        input
            .Begin("Input")
            .BindStaticFn("GetMovementX", Input::GetMovementX)
            .BindStaticFn("GetMovementY", Input::GetMovementY)
            .BindStaticFn("GetLookAroundX", Input::GetLookAroundX)
            .BindStaticFn("GetLookAroundY", Input::GetLookAroundY)
            .BindStaticFn("IsInteractPressed", Input::IsInteractPressed)
            .BindStaticFn("Jump", Input::Jump)
            .End();

        // Components
        LuaBinder<AnimationPlayer> animationPlayer(L);
        animationPlayer
            .Begin("AnimationPlayer")
            .BindMemFn("SetClip", &AnimationPlayer::SetClip)
            .BindMemFn("Play", &AnimationPlayer::Play)
            .End();

        LuaBinder<Camera> camera(L);
        camera
            .Begin("Camera")
            .BindMemFn("LookAt", &Camera::LookAt)
            .End();

        LuaBinder<Light> light(L);
        light
            .Begin("Light")
            .BindMemFn("GetIntensity", &Light::GetIntensity)
            .BindMemFn("SetIntensity", &Light::SetIntensity)
            .End();

        // ObjPtr
        LuaBinder<ObjPtr<Object>> objPtr(L);
        objPtr
            .Begin("ObjPtr")
            .BindStaticFn("New", [](lua_State* L) -> int { 
                    const char* typeName = luaL_checkstring(L, -1);
                    luaL_getmetatable(L, typeName);
                    if (lua_istable(L, -1))
                    {
                        LuaUserDataPack<ObjPtr<Object>>* m = (LuaUserDataPack<ObjPtr<Object>>*)lua_newuserdata(L, sizeof(LuaUserDataPack<ObjPtr<Object>>));
                        new (m)LuaUserDataPack<ObjPtr<Object>>();
                        m->dataType = LuaEngineUserDataType::ObjPtr;
                        lua_pushvalue(L, -2);
                        lua_setmetatable(L, -2);
                    }
                    else
                    {
                        lua_pop(L, 1);
                        lua_pushnil(L);
                    }
                    return 1;
            })
            .BindStaticFn("IsValid", [](ObjPtr<Object> val) { return val != nullptr; })
            .End();

        // Objects
        LuaBinder<Animation> animation(L);
            animation
            .Begin("Animation")
            .End();
        // clang-format on

        lua_setglobal(L, "wl");
    }
};
