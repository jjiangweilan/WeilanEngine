#include "Libs/Serialization/Serializable.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include "Object.hpp"
#include <functional>
#include <memory>
#pragma once

class AssetDatabase;
class Asset : public Object, public Serializable
{
public:
    void SetName(std::string_view name)
    {
        this->name = name;
        SetDirty();
    }
    const std::string& GetName() const
    {
        return name;
    }
    Asset() = default;
    Asset(const Asset& other) = default;
    Asset(Asset&& other) = default;
    virtual ~Asset(){};

    virtual void Reload(Asset&& asset)
    {
        uuid = std::move(asset.uuid);
        name = std::move(asset.name);
    }

    // asset format that is not serializable and deserializable.
    // AssetDatabase will use LoadFromFile if this function returns true when importing asset
    virtual bool IsExternalAsset()
    {
        return false;
    }

    // return false if loading failed
    virtual bool LoadFromFile(const char* path)
    {
        return false;
    }

    virtual std::vector<Asset*> GetInternalAssets()
    {
        return std::vector<Asset*>{};
    }

    bool IsDirty()
    {
        return isDirty;
    }

    virtual std::unique_ptr<Asset> Clone()
    {
        return nullptr;
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

    virtual void OnLoadingFinished() {}

    virtual const char* GetExtension() = 0;

    void SetDirty(bool isDirty = true)
    {
        this->isDirty = isDirty;
    }

    // used to identify if the asset contains the same content
    virtual uint32_t GetContentHash()
    {
        return 0;
    }

    // when dependent files or assets are updated, this can return true to indicate a import is needed
    // e.g. shader's included files
    virtual bool NeedReimport()
    {
        return false;
    }

protected:
    std::string name = "";

private:
    bool isDirty = false;
};

struct AssetRegistry
{
public:
    using Extension = std::string;
    using Creator = std::function<std::unique_ptr<Asset>()>;
    static std::unique_ptr<Asset> CreateAsset(const ObjectTypeID& id);
    static std::unique_ptr<Asset> CreateAssetByExtension(const Extension& id);
    template <class T>
    static std::unique_ptr<T> CreateAsset(const ObjectTypeID& id);
    static char RegisterAsset(const ObjectTypeID& assetID, const char* ext, const Creator& creator);
    static char RegisterExternalAsset(const ObjectTypeID& assetID, const char* ext, const Creator& creator);
    static bool IsExtensionAnAsset(const std::string& ext);

private:
    static std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Asset>()>>* GetAssetTypeRegistery();
    static std::unordered_map<Extension, std::function<std::unique_ptr<Asset>()>>* GetAssetExtensionRegistry();
};

template <class T>
concept IsAsset = requires { std::derived_from<T, Asset>; };

#define DECLARE_ASSET()                                                                                                \
    DECLARE_OBJECT()                                                                                                   \
public:                                                                                                                \
    const char* GetExtension() override;                                                                               \
    static const char* StaticGetExtension();                                                                           \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DECLARE_EXTERNAL_ASSET()                                                                                       \
    DECLARE_OBJECT()                                                                                                   \
public:                                                                                                                \
    const char* GetExtension() override;                                                                               \
    static const char* StaticGetExtension();                                                                           \
    bool IsExternalAsset() override                                                                                    \
    {                                                                                                                  \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DEFINE_ASSET(Type, ObjectID, Extension)                                                                        \
    DEFINE_OBJECT(Type, ObjectID)                                                                                      \
    char Type::_register = AssetRegistry::RegisterAsset(                                                               \
        Type::StaticGetObjectTypeID(),                                                                                 \
        StaticGetExtension(),                                                                                          \
        []() { return std::unique_ptr<Asset>(new Type()); }                                                            \
    );                                                                                                                 \
    const char* Type::GetExtension()                                                                                   \
    {                                                                                                                  \
        return StaticGetExtension();                                                                                   \
    }                                                                                                                  \
    const char* Type::StaticGetExtension()                                                                             \
    {                                                                                                                  \
        return "." Extension;                                                                                          \
    }
