#include "Object.hpp"

namespace Engine
{
std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Object>()>>* ObjectRegistry::GetObjectTypeRegistery()
{
    static std::unique_ptr<std::unordered_map<ObjectTypeID, ObjectRegistry::Creator>> registeredObject =
        std::make_unique<std::unordered_map<ObjectTypeID, ObjectRegistry::Creator>>();
    return registeredObject.get();
}

char ObjectRegistry::RegisterObject(const ObjectTypeID& ObjectID, const Creator& creator)
{
    GetObjectTypeRegistery()->emplace(ObjectID, creator);
    return '0';
}

std::unique_ptr<Object> ObjectRegistry::CreateObject(const ObjectTypeID& id)
{
    auto iter = GetObjectTypeRegistery()->find(id);
    if (iter != GetObjectTypeRegistery()->end())
    {
        return iter->second();
    }

    return nullptr;
}
} // namespace Engine
