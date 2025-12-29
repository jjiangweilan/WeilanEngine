#pragma once

#include "Engine/Core/ObjectTracker.hpp"
#include "Engine/Library/Serialization/Serializable.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Library/UUID.hpp"
#include "SafeReferenceable.hpp"
#include <spdlog/spdlog.h>
#include <unordered_map>

class Component;

// forward declared for type reflection
namespace TypeReflectionNS
{
bool RegisterMemberVariables();
}

class ObjectTypeInfo
{
public:
    const ObjectTypeInfo* GetParentTypeInfo() const { return parentTypeInfo; }
    const std::string& GetTypeName() const { return typeName; }
    const UUID& GetTypeID() const { return typeID; }
    std::unique_ptr<Object> CreateInstance() const
    {
        if (creator)
            return creator();

        return nullptr;
    }

private:
    ObjectTypeInfo* parentTypeInfo;
    std::string typeName;
    UUID typeID;
    std::function<std::unique_ptr<Object>()> creator;

    friend class ObjectRegistry;
};

using ObjectTypeID = UUID;
class Object : public Serializable, public SafeReferenceable<Object>
{
public:
    static const ObjectTypeID& StaticGetObjectTypeID();
    static const std::string& StaticGetTypeName();
    virtual const UUID& GetObjectTypeID() const;
    virtual const std::string& GetTypeName() const;
    friend bool TypeReflectionNS::RegisterMemberVariables();

private:
    static const char _objectRegister;

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
    static EngineObjectMap GetAllEngineObjects();
    template <class T>
    static std::vector<T*> GetObjectsOfType();

    virtual void SetName(std::string_view name) { this->name = name; }
    const std::string& GetName() const { return name; }

protected:
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

    std::string name;
    UUID uuid;

    friend class ObjectTracker;
    template <class T>
    friend class TypeReflection;
};

class ObjectRegistry
{
public:
    using Creator = std::function<std::unique_ptr<Object>()>;

    // ============ Functional APIs ============ //
    static std::unique_ptr<Object> CreateObject(const ObjectTypeID& id);
    template <class T>
    static std::unique_ptr<T> CreateObject(const ObjectTypeID& id);
    template <class T>
    static std::unique_ptr<T> CreateObject(std::string_view id);
    static std::unique_ptr<Object> CreateObjectByName(std::string_view name);

    // =========== Register Object ================ //
    template <class T>
    static char RegisterObject(const ObjectTypeID& parentID, const ObjectTypeID& objectID, std::string_view typeName, const Creator& creator)
    {
        auto self = GetObjectTypeInfoPrivate(objectID);
        auto parent = GetObjectTypeInfoPrivate(parentID);
        GetObjectTypeRegistryByName()->emplace(typeName, self);

        self->creator = creator;
        self->typeID = objectID;
        self->typeName = typeName;
        self->parentTypeInfo = parent;

        if (std::derived_from<T, Component>)
        {
            GetComponentTypeNamesRegistry().emplace_back(typeName);
        }
        return '0';
    }

    static char RegisterBaseObject(const ObjectTypeID& objectID, std::string_view typeName, const Creator& creator)
    {
        auto self = GetObjectTypeInfoPrivate(objectID);
        GetObjectTypeRegistryByName()->emplace(typeName, self);

        self->creator = creator;
        self->typeID = objectID;
        self->typeName = typeName;
        self->parentTypeInfo = nullptr;

        return '0';
    }

    static const std::vector<std::string>& GetComponentTypeNames() { return GetComponentTypeNamesRegistry(); }

    static const ObjectTypeInfo* GetObjectTypeInfo(const UUID& typeID);

private:
    static ObjectTypeInfo* GetObjectTypeInfoPrivate(const UUID& typeID);
    static std::unordered_map<ObjectTypeID, std::unique_ptr<ObjectTypeInfo>>* GetObjectTypeInfoRegistry();
    static std::unordered_map<std::string, ObjectTypeInfo*>* GetObjectTypeRegistryByName();
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

#define DECLARE_OBJECT()                                     \
                                                             \
public:                                                      \
    static const ObjectTypeID& StaticGetObjectTypeID();      \
    static const std::string& StaticGetTypeName();           \
    const std::string& GetTypeName() const override;         \
    const ObjectTypeID& GetObjectTypeID() const override;    \
                                                             \
    friend bool TypeReflectionNS::RegisterMemberVariables(); \
                                                             \
private:                                                     \
    static const char _objectRegister;

#define DEFINE_OBJECT(Parent, Type, ObjectID)                                \
    const char Type::_objectRegister = ObjectRegistry::RegisterObject<Type>( \
        Parent::StaticGetObjectTypeID(),                                     \
        StaticGetObjectTypeID(),                                             \
        #Type,                                                               \
        []() { \
        if constexpr (std::is_abstract_v<Type>) { \
            return nullptr; \
        } \
        else { \
            return std::unique_ptr<Object>(new Type()); \
        } }                                                             \
    );                                                                       \
    const ObjectTypeID& Type::StaticGetObjectTypeID()                        \
    {                                                                        \
        static const UUID uuid = UUID(ObjectID);                             \
        return uuid;                                                         \
    }                                                                        \
    const ObjectTypeID& Type::GetObjectTypeID() const                        \
    {                                                                        \
        return Type::StaticGetObjectTypeID();                                \
    }                                                                        \
    const std::string& Type::StaticGetTypeName()                             \
    {                                                                        \
        static std::string typeName = #Type;                                 \
        return typeName;                                                     \
    }                                                                        \
    const std::string& Type::GetTypeName() const                             \
    {                                                                        \
        return StaticGetTypeName();                                          \
    }

template <class T>
std::vector<T*> Object::GetObjectsOfType()
{
    std::vector<T*> result;
    auto objs = GetAllEngineObjects();
    for (auto obj : objs)
    {
        auto cast = dynamic_cast<T*>(obj.second);
        if (cast)
            result.push_back(cast);
    }

    return result;
}
