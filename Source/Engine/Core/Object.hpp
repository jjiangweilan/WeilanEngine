#pragma once

#include "Libs/Serialization/Serializer.hpp"
#include "Libs/UUID.hpp"
#include <string>
#include <typeinfo>
namespace Engine
{
class Object
{
public:
    virtual ~Object(){};

    const UUID& GetUUID() { return uuid; }
    void SetUUID(const UUID& uuid) { this->uuid = uuid; }

protected:
    UUID uuid;
};
} // namespace Engine
