#pragma once
#include "Serializer.hpp"
namespace Engine
{

// future-proof conversion define
// for now it just pass the value through
#define GENERATE_SERIALIZE_ID(x) x

class Serializable
{
    virtual void Serialize(Serializer* s) = 0;
    virtual void Deserialize(Serializer* s) = 0;
};

} // namespace Engine
