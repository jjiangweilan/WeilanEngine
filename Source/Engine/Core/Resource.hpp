#include "Libs/Serialization/Serializable.hpp"
#include <functional>
#include <memory>
#pragma once

namespace Engine
{

using ResourceID = std::string;

class Resource : public Serializable
{
public:
    const UUID& GetUUID() { return uuid; }
    void SetUUID(const UUID& uuid) { this->uuid = uuid; }
    virtual ~Resource(){};
    virtual void Serialize(Serializer* s) { s->Serialize(uuid); }
    virtual void Deserialize(Serializer* s) { s->Deserialize(uuid); }

protected:
    UUID uuid;
};

struct ResourceRegister
{
public:
    static std::unique_ptr<Resource> CreateResource(const ResourceID& id);

protected:
    static char RegisterResource(const ResourceID& id, const std::function<std::unique_ptr<Resource>()>& f);

private:
    static const std::unique_ptr<std::unordered_map<ResourceID, std::function<std::unique_ptr<Resource>()>>>&
    GetResourceRegisters();
    static std::unique_ptr<std::unordered_map<ResourceID, std::function<std::unique_ptr<Resource>()>>>
        resourceRegisters;
};

template <class T>
struct ResourceMeta : ResourceRegister
{};

#define REGISTER_RESOURCE(Type)                                                                                        \
    template <>                                                                                                        \
    struct ResourceMeta<Type> : ResourceRegister                                                                       \
    {                                                                                                                  \
        inline static const ResourceID resourceID = #Type;                                                             \
        inline static const char resourceRegister =                                                                    \
            RegisterResource(resourceID, []() { return std::unique_ptr<Resource>(new Type); });                        \
    };
} // namespace Engine
