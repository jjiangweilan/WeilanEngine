#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Object.hpp"
#include <functional>
#include <memory>
#pragma once

namespace Engine
{

using ResourceID = std::string;

class Resource : public Object
{
public:
    void SetName(std::string_view name) { this->name = name; }
    const std::string& GetName() { return name; }
    virtual ~Resource(){};
    virtual void Reload(Resource&& resource)
    {
        uuid = std::move(resource.uuid);
        name = std::move(resource.name);
    }

protected:
    std::string name;

    friend struct SerializableField<Resource>;
};

using AssetID = std::string;
struct AssetRegister
{
protected:
    using Creator = std::function<std::unique_ptr<Resource>()>;
    static char RegisterAsset(const AssetID& assetID, const Creator& creator);
    static std::unordered_map<AssetID, std::function<std::unique_ptr<Resource>()>> registeredAsset;
};

template <class T>
struct SerializableAsset : public AssetRegister
{
    // inline static AssetID assetID = GENERATE_SERIALIZABLE_FILE_ID("xxx");
    // inline static char reg = RegisterAsset(GetAssetID(), []() { return std::make_unique<T>(); })
};

template <>
struct SerializableField<Resource>
{
    static void Serialize(Resource* v, Serializer* s) { s->Serialize(v->uuid); }
    static void Deserialize(Resource* v, Serializer* s) { s->Deserialize(v->name); }
};

template <class T>
concept IsResource = requires { std::derived_from<T, Resource>; };

} // namespace Engine
