#pragma once

#include "Libs/UUID.hpp"
#include <string>
#include <typeinfo>
#include <unordered_map>
namespace Engine
{
class Object
{
public:
    virtual ~Object(){};

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
    static char RegisterObject(const ObjectTypeID& ObjectID, const Creator& creator);

private:
    static std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Object>()>>* GetObjectTypeRegistery();
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
    const char Type::_objectRegister =                                                                                 \
        ObjectRegistry::RegisterObject(StaticGetObjectTypeID(), []() { return std::unique_ptr<Object>(new Type()); }); \
    const ObjectTypeID& Type::StaticGetObjectTypeID()                                                                  \
    {                                                                                                                  \
        static const UUID uuid = UUID(ObjectID);                                                                       \
        return uuid;                                                                                                   \
    }                                                                                                                  \
    const ObjectTypeID& Type::GetObjectTypeID()                                                                        \
    {                                                                                                                  \
        return Type::StaticGetObjectTypeID();                                                                          \
    }
} // namespace Engine
