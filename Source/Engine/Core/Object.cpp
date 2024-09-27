#include "Object.hpp"

std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Object>()>>* ObjectRegistry::GetObjectTypeRegistry()
{
    static std::unique_ptr<std::unordered_map<ObjectTypeID, ObjectRegistry::Creator>> registeredObject =
        std::make_unique<std::unordered_map<ObjectTypeID, ObjectRegistry::Creator>>();
    return registeredObject.get();
}

// char ObjectRegistry::RegisterObject(const ObjectTypeID& ObjectID, std::string_view typeName, const Creator& creator)
// {
//     GetObjectTypeRegistry()->emplace(ObjectID, creator);
//     GetObjectTypeRegistryByName()->emplace(typeName, creator);
//     return '0';
// }

std::unordered_map<std::string, std::function<std::unique_ptr<Object>()>>* ObjectRegistry::GetObjectTypeRegistryByName()
{
    static std::unique_ptr<std::unordered_map<std::string, ObjectRegistry::Creator>> registeredObject =
        std::make_unique<std::unordered_map<std::string, ObjectRegistry::Creator>>();
    return registeredObject.get();
}

std::unique_ptr<Object> ObjectRegistry::CreateObject(const ObjectTypeID& id)
{
    auto iter = GetObjectTypeRegistry()->find(id);
    if (iter != GetObjectTypeRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}

std::vector<std::string>& ObjectRegistry::GetComponentTypeNamesRegistry()
{
    static std::vector<std::string> s{};
    return s;
}
