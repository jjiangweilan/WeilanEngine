#pragma once
#include "Libs/For_Each_Argument.hpp"
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

#define DECLARE_SERIALIZATION()                                                                                        \
    void Serialize(Serializer* ser) const override;                                                                    \
    void Deserialize(Serializer* ser) override;

#define SER(x) SerializationPack(#x, &x)

#define DEFINE_SERIALIZATION(TypeName, ...)                                                                            \
    void TypeName::Serialize(Serializer* ser) const                                                                    \
    {                                                                                                                  \
        [&](auto&&... fields)                                                                                          \
        {                                                                                                              \
            int namesIdx = 0;                                                                                          \
            for_each_argument([&namesIdx, ser](auto&& arg) { ser->Serialize(arg.name, arg.val); }, fields...);         \
        }(__VA_ARGS__);                                                                                                \
    }                                                                                                                  \
    void TypeName::Deserialize(Serializer* ser)                                                                        \
    {                                                                                                                  \
        [&](auto&&... fields)                                                                                          \
        {                                                                                                              \
            int namesIdx = 0;                                                                                          \
            for_each_argument([&namesIdx, ser](auto&& arg) { ser->Deserialize(arg.name, arg.val); }, fields...);       \
        }(__VA_ARGS__);                                                                                                \
    }
