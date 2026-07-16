#include "Engine/Library/EnumFlags.hpp"
#include "Engine/Library/Serialization/Serializable.hpp"
#include "Engine/Library/Serialization/Serializer.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include "Object.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#pragma once

class AssetDatabase;
enum class AssetState
{
    None = 0,
    DontSave = 1,
};
ENUM_FLAGS(AssetState, int);

class WEILAN_ENGINE_API Asset : public Object
{
public:
    void SetName(std::string_view name) override
    {
        Object::SetName(name);
        SetDirty();
    }
    void SetName_Lua(const std::string& name) { SetName(name); }
    Asset() = default;
    Asset(const Asset& other) = default;
    Asset(Asset&& other) = default;
    virtual ~Asset() {};

    [[deprecated("we should replace this with ObjPtr")]]
    virtual void Reload(Asset&& asset)
    {
        // uuid = std::move(asset.uuid); reload should change uuid, it's managed by AssetData
        name = std::move(asset.name);
    }

    // External assets are imported from formats that the engine does not save directly.
    virtual bool IsExternalAsset() { return false; }

    // Custom loaders may use this for assets whose source representation is not JSON.
    virtual bool LoadFromFile(const char* path) { return false; }

    // Writes the asset's source representation. The default implementation uses JSON serialization.
    virtual bool SaveToFile(const std::filesystem::path& path) const;

    virtual std::vector<Asset*> GetInternalAssets() { return std::vector<Asset*>{}; }

    bool IsDirty() { return HasFlag(stateFlags, AssetState::DontSave) ? false : isDirty; }

    virtual std::unique_ptr<Asset> Clone() { return nullptr; }

    void SetFlags(AssetStateFlags flags) { this->stateFlags |= flags; }
    void UnsetFlags(AssetStateFlags flags) { this->stateFlags &= ~flags; }
    AssetStateFlags GetFlags() const { return stateFlags; }

    void Serialize(Serializer* s) const override { Object::Serialize(s); }

    void Deserialize(Serializer* s) override { Object::Deserialize(s); }

    virtual void OnLoaded() {}

    virtual const std::string& GetExtension() = 0;

    void SetDirty(bool isDirty = true) { this->isDirty = isDirty; }

    // used to identify if the asset contains the same content
    virtual uint32_t GetContentHash() { return 0; }

    // when dependent files or assets are updated, this can return true to indicate a import is needed
    // e.g. shader's included files
    virtual bool NeedReimport() { return false; }

protected:
    std::string name = "";
    AssetPath sourceAssetFile;

    static std::vector<std::string> GenerateExtensions(const std::string& exts, char delimiter)
    {
        auto tokens = Utils::SplitString(exts, ',');
        for (auto& t : tokens)
        {
            t = "." + t;
        }
        return tokens;
    }

private:
    bool isDirty = false;
    AssetStateFlags stateFlags = AssetState::None;

    friend class ObjectReflection;
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
    static char RegisterAsset(
        const ObjectTypeID& assetID, const std::vector<std::string>& exts, const Creator& creator
    );
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
    const std::string& GetExtension() override;                                                                        \
    static const std::vector<std::string>& StaticGetExtensions();                                                      \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DECLARE_EXTERNAL_ASSET()                                                                                       \
    DECLARE_OBJECT()                                                                                                   \
public:                                                                                                                \
    const std::string& GetExtension() override;                                                                        \
    static const std::vector<std::string>& StaticGetExtensions();                                                      \
    bool IsExternalAsset() override                                                                                    \
    {                                                                                                                  \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DEFINE_ASSET(Type, ObjectID, Extension)                                                                        \
    DEFINE_OBJECT(Asset, Type, ObjectID)                                                                               \
    char Type::_register = AssetRegistry::RegisterAsset(                                                               \
        Type::StaticGetObjectTypeID(),                                                                                 \
        StaticGetExtensions(),                                                                                         \
        []() { return std::unique_ptr<Asset>(new Type()); }                                                            \
    );                                                                                                                 \
    const std::string& Type::GetExtension()                                                                            \
    {                                                                                                                  \
        return StaticGetExtensions()[0];                                                                               \
    }                                                                                                                  \
    const std::vector<std::string>& Type::StaticGetExtensions()                                                        \
    {                                                                                                                  \
        static std::vector<std::string> extensions = GenerateExtensions(Extension, ',');                               \
        return extensions;                                                                                             \
    }
