#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Object.hpp"
#include <functional>
#include <memory>
#pragma once

namespace Engine
{

using ResourceID = std::string;

class Resource : public Object, Serializable
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

    void Serialize(Serializer* s) override
    {
        s->Serialize(uuid);
        s->Serialize(name);
    }

    void Deserialize(Serializer* s) override
    {
        s->Serialize(uuid);
        s->Deserialize(name);
    }

protected:
    std::string name;
};

using AssetTypeID = std::string;
struct AssetRegister
{
public:
    static std::unique_ptr<Resource> CreateAsset(const AssetTypeID& id);

protected:
    using Creator = std::function<std::unique_ptr<Resource>()>;
    static char RegisterAsset(const AssetTypeID& assetID, const Creator& creator);

private:
    static std::unordered_map<AssetTypeID, std::function<std::unique_ptr<Resource>()>>* GetRegisteredAsset();
};

template <class T>
struct AssetFactory : public AssetRegister
{
    // inline static AssetID assetTypeID = GENERATE_SERIALIZABLE_FILE_ID("xxx");
    // inline static char reg = RegisterAsset(GetAssetID(), []() { return std::make_unique<T>(); })
};

template <class T>
concept IsResource = requires { std::derived_from<T, Resource>; };

} // namespace Engine
