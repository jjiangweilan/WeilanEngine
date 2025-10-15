#pragma once
#include "Libs/CppUtility.hpp"
#include <string>
#include <unordered_map>
class Serializer;
class Serializable
{
public:
    virtual void Serialize(Serializer* s) const = 0;
    virtual void Deserialize(Serializer* s) = 0;
    virtual ~Serializable() {};
};

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

#define SER1(x) SerializationPack(#x, &x)
#define SER2(name, x) SerializationPack(#name, &x)
#define SER_EXPAND(x) x
#define GET_MACRO(_1, _2, name, ...) name
#define SER(...) SER_EXPAND(GET_MACRO(__VA_ARGS__, SER2, SER1)(__VA_ARGS__))

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
