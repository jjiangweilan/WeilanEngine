#pragma once
#include "Engine/CodeGen/Serialization_Generated.hpp"
#include "Engine/Library/CppUtility.hpp"
#include <concepts>
#include <string>
#include <unordered_map>
class Serializer;
class Serializable
{
public:
    virtual void SerializeByReflection(Serializer* s) {};
    virtual void DeserializeByReflection(Serializer* s) {};
    virtual void Serialize(Serializer* s) const = 0;
    virtual void Deserialize(Serializer* s) = 0;
    virtual ~Serializable() {};
};

template <class T>
concept IsSerializableClass = std::derived_from<T, Serializable>;

template <class T>
concept HasSerializeFunc = requires(T a, Serializer* s) {
    a.Serialize(s);
    a.Deserialize(s);
};

template <class T>
concept HasFreeSerializeFunc = requires(T* a, Serializer* s) {
    ::Serialize(s, a);
    ::Deserialize(s, a);
};

template <class T>
concept IsSerializable =
    IsSerializableClass<T> ||
    HasSerializeFunc<T> || HasFreeSerializeFunc<T>; // use with CanBeSerializerParameter for full test, Serializer itself uses this only

#define SERIALIZE(ser, name) ser->Serialize(#name, name)
#define DESERIALIZE(ser, name) ser->Deserialize(#name, name)

template <class T>
struct SerializationPack
{
    SerializationPack(const char* name, T* val) : name(name), val(val) {}
    const char* name;
    T* val;
};

#define DECLARE_SERIALIZATION()                     \
    void Serialize(Serializer* ser) const override; \
    void Deserialize(Serializer* ser) override;

#define DECLARE_SERIALIZABLE()             \
    void Serialize(Serializer* ser) const; \
    void Deserialize(Serializer* ser);

#define SER1(x) SerializationPack(#x, &x)
#define SER2(name, x) SerializationPack(#name, &x)
#define SER_EXPAND(x) x
#define GET_MACRO(_1, _2, name, ...) name
#define SER(...) SER_EXPAND(GET_MACRO(__VA_ARGS__, SER2, SER1)(__VA_ARGS__))

#define INLINE_DEFINE_SERIALIZABLE(...)                                                                              \
    void Serialize(Serializer* ser) const                                                                            \
    {                                                                                                                \
        [&](auto&&... fields)                                                                                        \
        { for_each_argument([ser](auto&& arg) { ser->Serialize(arg.name, *arg.val); }, fields...); }(__VA_ARGS__);   \
    }                                                                                                                \
    void Deserialize(Serializer* ser)                                                                                \
    {                                                                                                                \
        [&](auto&&... fields)                                                                                        \
        { for_each_argument([ser](auto&& arg) { ser->Deserialize(arg.name, *arg.val); }, fields...); }(__VA_ARGS__); \
    }

#define DEFINE_SERIALIZATION(TypeName, Parent, ...)                                                                  \
    void TypeName::Serialize(Serializer* ser) const                                                                  \
    {                                                                                                                \
        Parent::Serialize(ser);                                                                                      \
        [&](auto&&... fields)                                                                                        \
        { for_each_argument([ser](auto&& arg) { ser->Serialize(arg.name, *arg.val); }, fields...); }(__VA_ARGS__);   \
    }                                                                                                                \
    void TypeName::Deserialize(Serializer* ser)                                                                      \
    {                                                                                                                \
        Parent::Deserialize(ser);                                                                                    \
        [&](auto&&... fields)                                                                                        \
        { for_each_argument([ser](auto&& arg) { ser->Deserialize(arg.name, *arg.val); }, fields...); }(__VA_ARGS__); \
    }
