#pragma once

#include "Libs/UUID.hpp"
#include "SafeReferenceable.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>

class Component;

class Object : public SafeReferenceable<Object>
{
public:
    using EngineObjectMap = std::unordered_map<UUID, Object*>;

    Object();
    Object(Object&& other) : uuid(std::exchange(other.uuid, UUID::GetEmptyUUID())) {}
    Object(const Object& other) : uuid() {};
    virtual ~Object();

    const UUID& GetUUID() const { return uuid; }
    void SetUUID(const UUID& uuid)
    {
        if (this->uuid == uuid)
            return;

#if ENGINE_DEV_BUILD
        if (GetAllEngineObjects().find(uuid) != GetAllEngineObjects().end())
        {
            spdlog::error("making object with duplicated UUID");
        }
        else
#endif
        {
            GetAllEngineObjects().erase(selfIterator);
            selfIterator = GetAllEngineObjects().emplace(uuid, this).first;
            this->uuid = uuid;
        }
    }

    virtual const UUID& GetObjectTypeID() = 0;

    static EngineObjectMap& GetAllEngineObjects();
    template <class T>
    static std::vector<T*> GetObjectsOfType();

protected:
    UUID uuid;

private:
    EngineObjectMap::const_iterator selfIterator;
};

using ObjectTypeID = UUID;
struct ObjectRegistry
{
public:
    using Creator = std::function<std::unique_ptr<Object>()>;
    static std::unique_ptr<Object> CreateObject(const ObjectTypeID& id);
    template <class T>
    static std::unique_ptr<T> CreateObject(const ObjectTypeID& id);
    template <class T>
    static std::unique_ptr<T> CreateObject(std::string_view id);
    template <class T>
    static char RegisterObject(const ObjectTypeID& objectID, std::string_view typeName, const Creator& creator)
    {
        GetObjectTypeRegistry()->emplace(objectID, creator);
        GetObjectTypeRegistryByName()->emplace(typeName, creator);
        GetObjectTypeToTypeNameMap()->emplace(objectID, typeName);

        if (std::derived_from<T, Component>)
        {
            GetComponentTypeNamesRegistry().emplace_back(typeName);
        }
        return '0';
    }

    static const std::vector<std::string>& GetComponentTypeNames() { return GetComponentTypeNamesRegistry(); }
    static const std::string& GetTypeName(const ObjectTypeID& id)
    {
        auto iter = GetObjectTypeToTypeNameMap()->find(id);
        if (iter == GetObjectTypeToTypeNameMap()->end())
        {
            static std::string invalid = "Invalid Type";
            return invalid;
        }
        return iter->second;
    }

private:
    static std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Object>()>>* GetObjectTypeRegistry();
    static std::unordered_map<std::string, std::function<std::unique_ptr<Object>()>>* GetObjectTypeRegistryByName();
    static std::unordered_map<ObjectTypeID, std::string>* GetObjectTypeToTypeNameMap();
    static std::vector<std::string>& GetComponentTypeNamesRegistry();
};

template <class T>
concept IsObject = requires { std::derived_from<T, Object>; };

template <class T>
std::unique_ptr<T> ObjectRegistry::CreateObject(const ObjectTypeID& id)
{
    std::unique_ptr<Object> uptr = CreateObject(id);
    T* ptr = static_cast<T*>(uptr.release());
    return std::unique_ptr<T>(ptr);
}

#define DECLARE_OBJECT()                                                                                               \
public:                                                                                                                \
    static const ObjectTypeID& StaticGetObjectTypeID();                                                                \
    const ObjectTypeID& GetObjectTypeID() override;                                                                    \
                                                                                                                       \
private:                                                                                                               \
    static const char _objectRegister;

#define DEFINE_OBJECT(Type, ObjectID)                                                                                  \
    const char Type::_objectRegister = ObjectRegistry::RegisterObject<Type>(                                           \
        StaticGetObjectTypeID(),                                                                                       \
        #Type,                                                                                                         \
        []() { return std::unique_ptr<Object>(new Type()); }                                                           \
    );                                                                                                                 \
    const ObjectTypeID& Type::StaticGetObjectTypeID()                                                                  \
    {                                                                                                                  \
        static const UUID uuid = UUID(ObjectID);                                                                       \
        return uuid;                                                                                                   \
    }                                                                                                                  \
    const ObjectTypeID& Type::GetObjectTypeID()                                                                        \
    {                                                                                                                  \
        return Type::StaticGetObjectTypeID();                                                                          \
    }

template <class T>
std::vector<T*> Object::GetObjectsOfType()
{
    std::vector<T*> result;
    auto& objs = GetAllEngineObjects();
    for (auto obj : objs)
    {
        auto cast = dynamic_cast<T*>(obj.second);
        if (cast)
            result.push_back(cast);
    }

    return result;
}
