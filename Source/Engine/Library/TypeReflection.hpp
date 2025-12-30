#pragma once
#include "Engine/Library/CppUtility.hpp"

#include <functional>
#include <span>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

class Object;
struct FunctionMetadata
{
    std::string name;
    std::function<void(void*, void*, void**, size_t)> func;
    const std::type_info* returnType;
    std::vector<const std::type_info*> argTypes;
    size_t argCount;
};

struct PropertyMetadata
{
    std::string name;
    const std::type_info* typeInfo;

    std::function<void(void*, void*&)> getter;
    std::function<void(void*, void*)> copyOperator;
};

class ITypeReflection
{
public:
    virtual const std::unordered_map<std::string, PropertyMetadata>&
    GetVariables() = 0;

    virtual void Copy(Object* src, Object* dst) const = 0;
    virtual void GetVariable(Object& obj, const std::string& name, void* ptr) const = 0;
    virtual void CallFunction(
        Object& obj,
        const std::string& name,
        void* rtnPtr,
        void** argPtrs,
        size_t argCount
    ) const = 0;
};

template <class T>
class TypeReflection : public ITypeReflection
{
public:
    const std::unordered_map<std::string, PropertyMetadata>&
    GetVariables() override
    {
        return StaticGetVariables();
    }

    void Copy(Object* src, Object* dst) const override
    {
        if (src && dst)
        {
            for (const auto& varPair : StaticGetVariables())
            {
                const auto& typeInfo = varPair.second.typeInfo;
                const auto& getter = varPair.second.getter;
                const auto& copyOperator = varPair.second.copyOperator;

                void* srcVal = nullptr;
                getter((void*)src, srcVal);

                void* dstVal = nullptr;
                getter((void*)dst, dstVal);

                if (srcVal != nullptr && dstVal != nullptr)
                    copyOperator(srcVal, dstVal);
            }
        }
    }

    void GetVariable(Object& obj, const std::string& name, void* ptr) const override
    {
        auto iter = GetVariablesPrivate().find(name);

        if (iter == GetVariablesPrivate().end())
        {
            return;
        }

        auto& metaData = iter->second;
        auto& f = metaData.getter;
        f(&obj, ptr);
    }

    void CallFunction(
        Object& obj,
        const std::string& name,
        void* rtnPtr,
        void** argPtrs,
        size_t argCount
    ) const override
    {
        auto iter = GetFunctionsPrivate().find(name);
        if (iter == GetFunctionsPrivate().end())
        {
            return;
        }

        auto& metadata = iter->second;
        auto& f = metadata.func;
        f(&obj, rtnPtr, argPtrs, argCount);
    }

public:
    template <class MemType>
    static void RegisterMemberVariable(const std::string& name, MemType T::* memPtr)
    {
        ASSERT(GetVariablesPrivate().find(name) == GetVariablesPrivate().end());

        GetVariablesPrivate()[name] = {
            name,
            &typeid(MemType),
            [memPtr](void* obj, void*& val)
            { val = &(static_cast<T*>((Object*)obj)->*memPtr); },
            [memPtr](void* src, void* dst)
            { *((MemType*)dst) = *((MemType*)src); }
        };
    }

    template <class Rtn, class... Args>
    static void RegisterMemberFunction(const std::string& name, Rtn (T::*funcPtr)(Args...))
    {
        ASSERT(GetFunctionsPrivate().find(name) == GetFunctionsPrivate().end());

        GetFunctionsPrivate()[name] = {
            name,
            [funcPtr](void* obj, void* rtnPtr, void** argPtrs, size_t argCount)
            {
                ASSERT(argCount == sizeof...(Args));

                if constexpr (std::is_void_v<Rtn>)
                {
                    CallMemberFunctionImpl(obj, funcPtr, argPtrs, std::index_sequence_for<Args...>{});
                }
                else
                {
                    Rtn result = CallMemberFunctionImpl(obj, funcPtr, argPtrs, std::index_sequence_for<Args...>{});
                    if (rtnPtr)
                    {
                        *static_cast<Rtn*>(rtnPtr) = result;
                    }
                }
            },
            &typeid(Rtn),
            {&typeid(Args)...},
            sizeof...(Args)
        };
    }

    template <class MemType>
    static MemType* GetVariable(T& obj, const std::string& name)
    {
        auto iter = GetVariablesPrivate().find(name);

        if (iter == GetVariablesPrivate().end())
        {
            return nullptr;
        }

        auto& metaData = iter->second;
        auto& typeInfo = metaData.typeInfo;
        if (typeid(MemType) != *typeInfo)
        {
            return nullptr;
        }

        auto& f = metaData.getter;
        void* val = nullptr;
        f((void*)&obj, val);

        return (MemType*)val;
    }

    template <class Rtn, class... Args>
    static Rtn CallFunction(T& obj, const std::string& name, Args... args)
    {
        auto iter = GetFunctionsPrivate().find(name);
        ASSERT(iter != GetFunctionsPrivate().end());

        auto& metadata = iter->second;

        // Type check: verify return type matches
        ASSERT(*metadata.returnType == typeid(Rtn));

        // Type check: verify argument count matches
        ASSERT(metadata.argCount == sizeof...(Args));

        // Type check: verify each argument type matches
        if constexpr (sizeof...(Args) > 0)
        {
            const std::type_info* callArgTypes[] = {&typeid(Args)...};
            for (size_t i = 0; i < sizeof...(Args); ++i)
            {
                ASSERT(*metadata.argTypes[i] == *callArgTypes[i]);
            }
        }

        auto& f = metadata.func;

        if constexpr (std::is_void_v<Rtn>)
        {
            // Handle void return type
            if constexpr (sizeof...(Args) == 0)
            {
                f((void*)&obj, nullptr, nullptr, 0);
            }
            else
            {
                void* argPtrs[] = {&args...};
                f((void*)&obj, nullptr, argPtrs, sizeof...(Args));
            }
        }
        else
        {
            // Handle non-void return type
            Rtn result;
            if constexpr (sizeof...(Args) == 0)
            {
                f((void*)&obj, &result, nullptr, 0);
            }
            else
            {
                void* argPtrs[] = {&args...};
                f((void*)&obj, &result, argPtrs, sizeof...(Args));
            }
            return result;
        }
    }

    static const std::unordered_map<std::string, PropertyMetadata>&
    StaticGetVariables()
    {
        return GetVariablesPrivate();
    }

private:
    static std::unordered_map<std::string, PropertyMetadata>& GetVariablesPrivate()
    {
        static std::unordered_map<std::string, PropertyMetadata> variables;
        return variables;
    }

    static std::unordered_map<std::string, FunctionMetadata>& GetFunctionsPrivate()
    {
        static std::unordered_map<std::string, FunctionMetadata> functions;
        return functions;
    }

    template <class Rtn, class... Args, size_t... Is>
    static Rtn CallMemberFunctionImpl(void* obj, Rtn (T::*funcPtr)(Args...), void** argPtrs, std::index_sequence<Is...>)
    {
        return (((Object*)obj)->*funcPtr)(*static_cast<std::remove_reference_t<Args>*>(argPtrs[Is])...);
    }
};

#define REGISTER_TYPE_REFLECTION_MEMBER_VARIABLE(Type, memName) \
    TypeReflection<Type>::RegisterMemberVariable(#memName, &Type::memName)

template <class T>
struct TypeReflectionPack
{
    TypeReflectionPack(const char* name, T val) : name(name), val(val) {}
    const char* name;
    T val;
};

#define TYPE_REFLECTION_MEM1(Type, x) TypeReflectionPack(#x, &Type::x)
#define TYPE_REFLECTION_MEM2(Type, name, x) TypeReflectionPack(#name, &Type::x)
#define TYPE_REFLECTION_MEM_EXPAND(x) x
#define TYPE_REFLECTION_GET_MACRO(_1, _2, name, ...) name
#define TYPE_REFLECTION_MEM(Type, ...) TYPE_REFLECTION_MEM_EXPAND(TYPE_REFLECTION_GET_MACRO(__VA_ARGS__, TYPE_REFLECTION_MEM2, TYPE_REFLECTION_MEM1)(Type, __VA_ARGS__))
#define TYPE_REFLECTION_MEMBER_VARIABLES(Type, ...)                                                                                          \
    namespace TypeReflectionNS                                                                                                               \
    {                                                                                                                                        \
    static bool RegisterMemberVariables()                                                                                                    \
    {                                                                                                                                        \
        [](auto&&... fields)                                                                                                                 \
        { for_each_argument([](auto&& arg) { TypeReflection<Type>::RegisterMemberVariable(arg.name, arg.val); }, fields...); }(__VA_ARGS__); \
        return true;                                                                                                                         \
    }                                                                                                                                        \
    static bool registered = RegisterMemberVariables();                                                                                      \
    }

#define REGISTER_TYPE_REFLECTION_MEMBER_FUNCTION(Type, funcName) \
    TypeReflection<Type>::RegisterMemberFunction(#funcName, &Type::funcName)

#define TYPE_REFLECTION_FUNC1(Type, x) TypeReflectionPack(#x, &Type::x)
#define TYPE_REFLECTION_FUNC2(Type, name, x) TypeReflectionPack(#name, &Type::x)
#define TYPE_REFLECTION_FUNC_EXPAND(x) x
#define TYPE_REFLECTION_FUNC_GET_MACRO(_1, _2, name, ...) name
#define TYPE_REFLECTION_FUNC(Type, ...) TYPE_REFLECTION_FUNC_EXPAND(TYPE_REFLECTION_FUNC_GET_MACRO(__VA_ARGS__, TYPE_REFLECTION_FUNC2, TYPE_REFLECTION_FUNC1)(Type, __VA_ARGS__))
#define TYPE_REFLECTION_MEMBER_FUNCTIONS(Type, ...)                                                                                          \
    namespace TypeReflectionNS                                                                                                               \
    {                                                                                                                                        \
    static bool RegisterMemberFunctions()                                                                                                    \
    {                                                                                                                                        \
        [](auto&&... fields)                                                                                                                 \
        { for_each_argument([](auto&& arg) { TypeReflection<Type>::RegisterMemberFunction(arg.name, arg.val); }, fields...); }(__VA_ARGS__); \
        return true;                                                                                                                         \
    }                                                                                                                                        \
    static bool registeredFuncs = RegisterMemberFunctions();                                                                                 \
    }
