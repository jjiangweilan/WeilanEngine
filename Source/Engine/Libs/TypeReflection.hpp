#pragma once
#include "Libs/CppUtility.hpp"

#include <functional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

template <class T>
class TypeReflection
{
public:
    template <class MemType>
    static void RegisterMemberVariable(const std::string& name, MemType T::* memPtr)
    {
        ASSERT(GetVariablesPrivate().find(name) == GetVariablesPrivate().end());

        GetVariablesPrivate()[name] = {
            &typeid(MemType),
            [memPtr](T& obj, void*& val)
            { val = &(obj.*memPtr); }
        };
    }

    template <class Rtn, class... Args>
    static void RegisterMemberFunction(const std::string& name, Rtn (T::*funcPtr)(Args...))
    {
        ASSERT(GetFunctionsPrivate().find(name) == GetFunctionsPrivate().end());

        GetFunctionsPrivate()[name] = {
            [funcPtr](T& obj, void* rtnPtr, void** argPtrs, size_t argCount)
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
            { &typeid(Args)... },
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

        auto& pair = iter->second;
        auto& typeInfo = pair.first;
        if (typeid(MemType) != *typeInfo)
        {
            return nullptr;
        }

        auto& f = pair.second;
        void* val = nullptr;
        f(obj, val);

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
            const std::type_info* callArgTypes[] = { &typeid(Args)... };
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
                f(obj, nullptr, nullptr, 0);
            }
            else
            {
                void* argPtrs[] = { &args... };
                f(obj, nullptr, argPtrs, sizeof...(Args));
            }
        }
        else
        {
            // Handle non-void return type
            Rtn result;
            if constexpr (sizeof...(Args) == 0)
            {
                f(obj, &result, nullptr, 0);
            }
            else
            {
                void* argPtrs[] = { &args... };
                f(obj, &result, argPtrs, sizeof...(Args));
            }
            return result;
        }
    }

    static const std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*&)>>>&
    GetVariables()
    {
        return GetVariablesPrivate();
    }

    // this can be used for derived class
    // template <class MemType>
    // bool Get(const std::string& name, MemType& val)
    // {
    //     return TypeReflection<T>::Get(*static_cast<T*>(this), name, val);
    // }

private:
    struct FunctionMetadata
    {
        std::function<void(T&, void*, void**, size_t)> func;
        const std::type_info* returnType;
        std::vector<const std::type_info*> argTypes;
        size_t argCount;
    };

    static std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*&)>>>& GetVariablesPrivate()
    {
        static std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*&)>>> variables;
        return variables;
    }

    static std::unordered_map<std::string, FunctionMetadata>& GetFunctionsPrivate()
    {
        static std::unordered_map<std::string, FunctionMetadata> functions;
        return functions;
    }

    template <class Rtn, class... Args, size_t... Is>
    static Rtn CallMemberFunctionImpl(T& obj, Rtn (T::*funcPtr)(Args...), void** argPtrs, std::index_sequence<Is...>)
    {
        return (obj.*funcPtr)(*static_cast<std::remove_reference_t<Args>*>(argPtrs[Is])...);
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
#define TYPE_REFLECTION_MEMBER_FUNCTIONS(Type, ...)                                                                                           \
    namespace TypeReflectionNS                                                                                                                \
    {                                                                                                                                         \
    static bool RegisterMemberFunctions()                                                                                                     \
    {                                                                                                                                         \
        [](auto&&... fields)                                                                                                                  \
        { for_each_argument([](auto&& arg) { TypeReflection<Type>::RegisterMemberFunction(arg.name, arg.val); }, fields...); }(__VA_ARGS__); \
        return true;                                                                                                                          \
    }                                                                                                                                         \
    static bool registeredFuncs = RegisterMemberFunctions();                                                                                  \
    }
