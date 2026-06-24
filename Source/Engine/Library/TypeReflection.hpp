#pragma once
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/CppUtility.hpp"
#include "Engine/Library/Serialization/Serializable.hpp" // for IsSerializable.

#include <functional>
#include <span>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

template <class T, class SerializerType>
concept IsBaseSerializationType = requires(T a, SerializerType* s) {
    s->Serialize("", a);
    s->Deserialize("", a);
};

class Object;
class Serializer;
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

    template <class T>
    bool IsType()
    {
        return *typeInfo == typeid(T);
    }

    std::function<void(void*, void*&)> getter;
    std::function<void(void*, void*)> copyOperator;
    std::function<void(std::string_view, void*, Serializer*)> serialize;
    std::function<void(std::string_view, void*, Serializer*)> deserialize;
};

class ITypeReflection
{
public:
    virtual const std::unordered_map<std::string, PropertyMetadata>&
    GetVariables() const = 0;
    virtual ~ITypeReflection() {};

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
concept IsCopyable = requires(T a, T b) {
    a = b;
};

// Define the concept
template <typename T>
struct is_vector_helper : std::false_type
{};

template <typename T, typename A>
struct is_vector_helper<std::vector<T, A>> : std::true_type
{};

template <typename T>
concept IsVector = is_vector_helper<T>::value;

// Define unordered_map concept
template <typename T>
struct is_unordered_map_helper : std::false_type
{};

template <typename K, typename V, typename H, typename E, typename A>
struct is_unordered_map_helper<std::unordered_map<K, V, H, E, A>> : std::true_type
{};

template <typename T>
concept IsUnorderedMap = is_unordered_map_helper<T>::value;

template <class T>
class TypeReflection : public ITypeReflection
{
public:
    const std::unordered_map<std::string, PropertyMetadata>&
    GetVariables() const override
    {
        return StaticGetVariables();
    }

    void Copy(Object* src, Object* dst) const override
    {
        if (src && dst)
        {
            for (const auto& varPair : StaticGetVariables())
            {
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
    template <class MemType, class SerializerType = Serializer>
    static void RegisterMemberVariable(const std::string& name, MemType T::* memPtr)
    {
        ASSERT(GetVariablesPrivate().find(name) == GetVariablesPrivate().end());

        GetVariablesPrivate()[name] = {
            .name = name,
            .typeInfo = &typeid(MemType),
            .getter = [memPtr](void* obj, void*& val)
            { val = &(static_cast<T*>((Object*)obj)->*memPtr); },
            .copyOperator = [memPtr](void* src, void* dst)
            { 
                auto copyItem = [](auto& srcItem, auto& dstItem)
                {
                    if constexpr (std::is_pointer_v<decltype(srcItem)>)
                    {
                        dstItem = srcItem; // For pointers, just copy the pointer
                    }
                    else if constexpr (std::is_copy_assignable_v<decltype(srcItem)>)
                    {
                        dstItem = srcItem; // For copy-assignable types, use assignment
                    }
                    // TODO: Handle Object using clone
                };

                if constexpr (IsVector<MemType>)
                {
                    // For vector, we need to copy each element
                    auto& srcVec = *((MemType*)src);
                    auto& dstVec = *((MemType*)dst);
                    dstVec.clear();
                    dstVec.resize(srcVec.size());
                    for (int i = 0; i < srcVec.size(); ++i)
                    {
                        copyItem(srcVec[i], dstVec[i]);
                    }
                }
                else if constexpr (IsUnorderedMap<MemType>)
                {
                    // For unordered_map, we need to copy each element
                    auto& srcMap = *((MemType*)src);
                    auto& dstMap = *((MemType*)dst);
                    dstMap.clear();
                    for (const auto& [key, value] : srcMap)
                    {
                        dstMap[key] = value;
                    }
                }
                else if constexpr (std::is_pointer_v<MemType>)
                {
                    *((MemType*)dst) = *((MemType*)src);
                }
                else if constexpr (std::is_copy_assignable_v<MemType>)
                {
                    *((MemType*)dst) = *((MemType*)src);
                } },
            .serialize = [memPtr](std::string_view name, void* obj, SerializerType* s)
            {
                if constexpr (IsSerializable<MemType> || IsBaseSerializationType<MemType, SerializerType>)
                {
                    MemType* val = &(static_cast<T*>((Object*)obj)->*memPtr);
                    s->Serialize(name, *val);
                } },
            .deserialize = [memPtr](std::string_view name, void* obj, SerializerType* s)
            {
                if constexpr (IsSerializable<MemType> || IsBaseSerializationType<MemType, SerializerType>)
                {
                    MemType* val = &(static_cast<T*>((Object*)obj)->*memPtr);
                    s->Deserialize(name, *val);
                } }
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
        return (static_cast<T*>((Object*)obj)->*funcPtr)(*static_cast<std::remove_reference_t<Args>*>(argPtrs[Is])...);
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
    bool Type::_RegisterMemberVariables()                                                                                                    \
    {                                                                                                                                        \
        [](auto&&... fields)                                                                                                                 \
        { for_each_argument([](auto&& arg) { TypeReflection<Type>::RegisterMemberVariable(arg.name, arg.val); }, fields...); }(__VA_ARGS__); \
        return true;                                                                                                                         \
    }                                                                                                                                        \
    static bool registered_##Type = Type::_RegisterMemberVariables();

#define REGISTER_TYPE_REFLECTION_MEMBER_FUNCTION(Type, funcName) \
    TypeReflection<Type>::RegisterMemberFunction(#funcName, &Type::funcName)

#define TYPE_REFLECTION_FUNC1(Type, x) TypeReflectionPack(#x, &Type::x)
#define TYPE_REFLECTION_FUNC2(Type, name, x) TypeReflectionPack(#name, &Type::x)
#define TYPE_REFLECTION_FUNC_EXPAND(x) x
#define TYPE_REFLECTION_FUNC_GET_MACRO(_1, _2, name, ...) name
#define TYPE_REFLECTION_FUNC(Type, ...) TYPE_REFLECTION_FUNC_EXPAND(TYPE_REFLECTION_FUNC_GET_MACRO(__VA_ARGS__, TYPE_REFLECTION_FUNC2, TYPE_REFLECTION_FUNC1)(Type, __VA_ARGS__))
#define TYPE_REFLECTION_MEMBER_FUNCTIONS(Type, ...)                                                                                          \
    bool Type::_RegisterMemberFunctions()                                                                                                    \
    {                                                                                                                                        \
        [](auto&&... fields)                                                                                                                 \
        { for_each_argument([](auto&& arg) { TypeReflection<Type>::RegisterMemberFunction(arg.name, arg.val); }, fields...); }(__VA_ARGS__); \
        return true;                                                                                                                         \
    }                                                                                                                                        \
    static bool registeredFuncs_##Type = Type::_RegisterMemberFunctions();\
