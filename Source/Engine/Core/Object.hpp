#pragma once

#include "Core/ObjectTracker.hpp"
#include "Libs/Serialization/Serializable.hpp"
#include "Libs/UUID.hpp"
#include "SafeReferenceable.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>

class Component;

class Object : public Serializable, public SafeReferenceable<Object>
{
public:
    using EngineObjectMap = std::unordered_map<UUID, Object*>;

    Object() { ObjectTracker::Singleton().AddObject(this); }

    Object(Object&& other) : name(std::move(other.name)), uuid()
    {
        ObjectTracker::Singleton().ReplaceObject(this, &other);
    }

    Object(const Object& other) : name(other.name), uuid() { ObjectTracker::Singleton().AddObject(this); };
    virtual ~Object() { ObjectTracker::Singleton().RemoveObject(this); }

    const UUID& GetUUID() const { return uuid; }
    void SetUUID(const UUID& uuid)
    {
        if (this->uuid == uuid)
            return;

        ObjectTracker::Singleton().RemoveObject(this);

        this->uuid = uuid;
        ObjectTracker::Singleton().AddObject(this);
    }

    virtual const UUID& GetObjectTypeID() = 0;
    virtual const std::string& GetTypeName() = 0;
    static const std::string& StaticGetTypeName()
    {
        static std::string typeName = "Object";
        return typeName;
    }
    static EngineObjectMap GetAllEngineObjects();
    template <class T>
    static DynamicArray<T*> GetObjectsOfType();

    virtual void SetName(std::string_view name) { this->name = name; }
    const std::string& GetName() const { return name; }

protected:
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    std::string name;
    UUID uuid;

    friend class ObjectTracker;
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
    static std::unique_ptr<Object> CreateObjectByName(std::string_view name);
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

    static const DynamicArray<std::string>& GetComponentTypeNames() { return GetComponentTypeNamesRegistry(); }
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
    static DynamicArray<std::string>& GetComponentTypeNamesRegistry();
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
                                                                                                                       \
public:                                                                                                                \
    static const ObjectTypeID& StaticGetObjectTypeID();                                                                \
    static const std::string& StaticGetTypeName();                                                                     \
    const std::string& GetTypeName() override;                                                                         \
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
    }                                                                                                                  \
    const std::string& Type::StaticGetTypeName()                                                                       \
    {                                                                                                                  \
        static std::string typeName = #Type;                                                                           \
        return typeName;                                                                                               \
    }                                                                                                                  \
    const std::string& Type::GetTypeName()                                                                             \
    {                                                                                                                  \
        return StaticGetTypeName();                                                                                    \
    }

template <class T>
DynamicArray<T*> Object::GetObjectsOfType()
{
    DynamicArray<T*> result;
    auto objs = GetAllEngineObjects();
    for (auto obj : objs)
    {
        auto cast = dynamic_cast<T*>(obj.second);
        if (cast)
            result.push_back(cast);
    }

    return result;
}
