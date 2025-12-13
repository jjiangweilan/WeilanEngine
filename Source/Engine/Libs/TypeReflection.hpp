#pragma once
#include "Libs/CppUtility.hpp"

#include <functional>
#include <string>
#include <typeinfo>
#include <unordered_map>

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
    static std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*&)>>>& GetVariablesPrivate()
    {
        static std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*&)>>> variables;
        return variables;
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
