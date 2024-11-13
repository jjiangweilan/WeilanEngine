#include "Object.hpp"

Object::EngineObjectMap& Object::GetAllEngineObjects()
{
    static EngineObjectMap allEngineObjects = EngineObjectMap();
    return allEngineObjects;
}

Object::Object()
{
#if ENGINE_DEV_BUILD
    if (GetAllEngineObjects().find(uuid) != GetAllEngineObjects().end())
    {
        spdlog::error("making object with duplicated UUID");
    }
    else
#endif
    {
        selfIterator = GetAllEngineObjects().emplace(uuid, this).first;
    }
}

Object::~Object()
{
    GetAllEngineObjects().erase(selfIterator);
}

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

std::unordered_map<ObjectTypeID, std::string>* ObjectRegistry::GetObjectTypeToTypeNameMap()
{
    static std::unordered_map<ObjectTypeID, std::string> registry;
    return &registry;
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
