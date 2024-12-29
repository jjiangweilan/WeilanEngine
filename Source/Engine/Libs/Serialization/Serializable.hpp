#pragma once
#include <string>
#include <unordered_map>
#define GENERATE_SERIALIZABLE_FILE_ID(x) x
class Serializer;
class Serializable
{
public:
    virtual void Serialize(Serializer* s) const = 0;
    virtual void Deserialize(Serializer* s) = 0;
    virtual ~Serializable(){};
};

#define SERIALIZE(ser, name) ser->Serialize(#name, name)
#define DESERIALIZE(ser, name) ser->Deserialize(#name, name)
