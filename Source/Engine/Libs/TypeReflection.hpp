#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <typeinfo>

template<class T>
class TypeReflection
{
public:
    template<class MemType>
    static void RegisterMemberVariable(const std::string& name, MemType T::* memPtr)
    {
        Singleton().variables[name] = { &typeid(MemType), [memPtr](T& obj, void* val) { *((MemType*)val) = obj.*memPtr; } };
    }

    template<class MemType>
    static bool Get(T& obj, const std::string& name, MemType& val)
    {
        auto iter = Singleton().variables.find(name);

        if (iter == Singleton().variables.end())
        {
            val = MemType();
            return false;
        }

        auto& pair = iter->second;
        auto& typeInfo = pair.first;
        if (typeid(MemType) != *typeInfo)
        {
            val = MemType();
            return false;
        }

        auto& f = pair.second;
        f(obj, &val);

        return true;
    }

    template<class MemType>
    bool Get(const std::string& name, MemType& val)
    {
        return TypeReflection<T>::Get(*static_cast<T*>(this), name, val);
    }

private:

    static TypeReflection<T>& Singleton()
    {
        static TypeReflection<T> instance;
        return instance;
    };

    std::unordered_map<std::string, std::pair<const std::type_info*, std::function<void(T&, void*)>>> variables;
};

#define REGISTER_TYPE_REFLECTION_MEMBER_VARIABLE(Type, memName) \
    TypeReflection<Type>::RegisterMemberVariable(#memName, &Type::memName)
