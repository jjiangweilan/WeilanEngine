#pragma once

#include "Libs/UUID.hpp"
#include "SafeReferenceable.hpp"
#include <unordered_map>

class Component;
class Object : public SafeReferenceable<Object>
{
public:
    Object() = default;
    Object(Object&& other) : uuid(std::exchange(other.uuid, UUID::GetEmptyUUID())) {}
    Object(const Object& other) : uuid() {};
    virtual ~Object() {};

    const UUID& GetUUID() const
    {
        return uuid;
    }
    void SetUUID(const UUID& uuid)
    {
        this->uuid = uuid;
    }

    virtual const UUID& GetObjectTypeID() = 0;

protected:
    UUID uuid;
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
    static char RegisterObject(const ObjectTypeID& ObjectID, std::string_view typeName, const Creator& creator)
    {
        GetObjectTypeRegistry()->emplace(ObjectID, creator);
        GetObjectTypeRegistryByName()->emplace(typeName, creator);

        if (std::derived_from<T, Component>)
        {
            GetComponentTypeNamesRegistry().emplace_back(typeName);
        }
        return '0';
    }

    static const std::vector<std::string>& GetComponentTypeNames()
    {
        return GetComponentTypeNamesRegistry();
    }

private:
    static std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Object>()>>* GetObjectTypeRegistry();
    static std::unordered_map<std::string, std::function<std::unique_ptr<Object>()>>* GetObjectTypeRegistryByName();
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
