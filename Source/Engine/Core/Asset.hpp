#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Object.hpp"
#include <functional>
#include <memory>
#pragma once

namespace Engine
{

using AssetTypeID = UUID;
class Asset : public Object, public Serializable
{
public:
    virtual const AssetTypeID& GetAssetTypeID() = 0;
    void SetName(std::string_view name)
    {
        this->name = name;
    }
    const std::string& GetName()
    {
        return name;
    }
    virtual ~Asset(){};

    virtual void Reload(Asset&& asset)
    {
        uuid = std::move(asset.uuid);
        name = std::move(asset.name);
    }

    // asset format that is not serializable and deserializable
    virtual bool IsExternalAsset()
    {
        return false;
    }

    // return false if loading failed
    virtual bool LoadFromFile(const char* path)
    {
        return false;
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

    virtual const char* GetExtension() = 0;

protected:
    std::string name;
};

struct AssetRegistry
{
public:
    using Extension = std::string;
    using Creator = std::function<std::unique_ptr<Asset>()>;
    static std::unique_ptr<Asset> CreateAsset(const AssetTypeID& id);
    static std::unique_ptr<Asset> CreateAssetByExtension(const Extension& id);
    template <class T>
    static std::unique_ptr<T> CreateAsset(const AssetTypeID& id);
    static char RegisterAsset(const AssetTypeID& assetID, const char* ext, const Creator& creator);
    static char RegisterExternalAsset(const AssetTypeID& assetID, const char* ext, const Creator& creator);

private:
    static std::unordered_map<AssetTypeID, std::function<std::unique_ptr<Asset>()>>* GetAssetTypeRegistery();
    static std::unordered_map<Extension, std::function<std::unique_ptr<Asset>()>>* GetAssetExtensionRegistry();
};

template <class T>
concept IsAsset = requires { std::derived_from<T, Asset>; };

template <class T>
std::unique_ptr<T> AssetRegistry::CreateAsset(const AssetTypeID& id)
{
    std::unique_ptr<Asset> uptr = CreateAsset(id);
    T* ptr = static_cast<T*>(uptr.release());
    return std::unique_ptr<T>(ptr);
}

#define DECLARE_ASSET()                                                                                                \
public:                                                                                                                \
    static const AssetTypeID& StaticGetAssetTypeID();                                                                  \
    const AssetTypeID& GetAssetTypeID() override;                                                                      \
    const char* GetExtension() override;                                                                               \
    static const char* StaticGetExtension();                                                                           \
                                                                                                                       \
private:                                                                                                               \
    static char _register;                                                                                             \
                                                                                                                       \
public:

#define DEFINE_ASSET(Type, AssetID, Extension)                                                                         \
    char Type::_register = AssetRegistry::RegisterAsset(                                                               \
        StaticGetAssetTypeID(),                                                                                        \
        StaticGetExtension(),                                                                                          \
        []() { return std::unique_ptr<Asset>(new Type()); }                                                            \
    );                                                                                                                 \
    const AssetTypeID& Type::StaticGetAssetTypeID()                                                                    \
    {                                                                                                                  \
        static const UUID uuid = UUID(AssetID);                                                                        \
        return uuid;                                                                                                   \
    }                                                                                                                  \
    const AssetTypeID& Type::GetAssetTypeID()                                                                          \
    {                                                                                                                  \
        return StaticGetAssetTypeID();                                                                                 \
    }                                                                                                                  \
    const char* Type::GetExtension()                                                                                   \
    {                                                                                                                  \
        return StaticGetExtension();                                                                                   \
    }                                                                                                                  \
    const char* Type::StaticGetExtension()                                                                             \
    {                                                                                                                  \
        return "." Extension;                                                                                          \
    }
} // namespace Engine
