#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Object.hpp"
#include <functional>
#include <memory>
#pragma once

namespace Engine
{

using ResourceTypeID = UUID;
class Resource : public Object, Serializable
{
public:
    virtual const ResourceTypeID& GetResourceTypeID() = 0;
    void SetName(std::string_view name)
    {
        this->name = name;
    }
    const std::string& GetName()
    {
        return name;
    }
    virtual ~Resource(){};
    virtual void Reload(Resource&& resource)
    {
        uuid = std::move(resource.uuid);
        name = std::move(resource.name);
    }

    void Serialize(Serializer* s) const override
    {
        s->Serialize("uuid", uuid);
        s->Serialize("name", name);
    }

    void Deserialize(Serializer* s) override
    {
        s->Deserialize("uuid", uuid);
        s->Deserialize("name", name);
    }

protected:
    std::string name;
};

struct ResourceRegistry
{
public:
    using Creator = std::function<std::unique_ptr<Resource>()>;
    static std::unique_ptr<Resource> CreateAsset(const ResourceTypeID& id);
    template <class T>
    static std::unique_ptr<T> CreateAsset(const ResourceTypeID& id);
    static char RegisterAsset(const ResourceTypeID& assetID, const Creator& creator);

private:
    static std::unordered_map<ResourceTypeID, std::function<std::unique_ptr<Resource>()>>* GetRegisteredAsset();
};

template <class T>
concept IsResource = requires { std::derived_from<T, Resource>; };

template <class T>
std::unique_ptr<T> ResourceRegistry::CreateAsset(const ResourceTypeID& id)
{
    std::unique_ptr<Resource> uptr = CreateAsset(id);
    T* ptr = static_cast<T*>(uptr.release());
    return std::unique_ptr<T>(ptr);
}

#define DECLARE_RESOURCE()                                                                                             \
public:                                                                                                                \
    static const ResourceTypeID& StaticGetResourceTypeID();                                                            \
    const ResourceTypeID& GetResourceTypeID() override;                                                                \
                                                                                                                       \
private:                                                                                                               \
    static char _register;                                                                                             \
                                                                                                                       \
public:

#define DEFINE_RESOURCE(Type, resourceID)                                                                              \
    char Type::_register = ResourceRegistry::RegisterAsset(                                                            \
        StaticGetResourceTypeID(),                                                                                     \
        []() { return std::unique_ptr<Resource>(new Type()); }                                                         \
    );                                                                                                                 \
    const ResourceTypeID& Type::StaticGetResourceTypeID()                                                              \
    {                                                                                                                  \
        static const UUID uuid = UUID(resourceID);                                                                     \
        return uuid;                                                                                                   \
    }                                                                                                                  \
    const ResourceTypeID& Type::GetResourceTypeID()                                                                    \
    {                                                                                                                  \
        return StaticGetResourceTypeID();                                                                              \
    }
} // namespace Engine
