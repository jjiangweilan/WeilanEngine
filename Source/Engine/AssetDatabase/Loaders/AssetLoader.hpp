#pragma once
#include "AssetDatabase/Private/AssetData.hpp"
#include "AssetDatabase/Private/ImportDatabase.hpp"
#include "Core/Asset.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <typeindex>

class AssetLoader
{
protected:
    // asset to import
    std::filesystem::path absoluteAssetPath{};

    std::vector<std::unique_ptr<AssetLoader>> dependencies;
    const ImportDatabase* importDatabase;
    AssetMeta meta;

public:
    virtual ~AssetLoader() {}
    void Setup(const ImportDatabase* importDatabase, const std::filesystem::path& assetPath, const AssetMeta& meta)
    {
        this->absoluteAssetPath = assetPath;
        this->importDatabase = importDatabase;
        this->meta = meta;
    }

    virtual bool IsInternalAsset() { return false; }

    virtual void Load() = 0;

    // no need to override if this data import doesn't need reference resolving
    virtual void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap)
    {
        serializer = nullptr;
        resolveMap = nullptr;
    }
    virtual std::unique_ptr<Asset> RetrieveAsset() = 0;

    // reload is called after RetrieveAsset so the loaded object is passed from outside
    virtual void HandleReload(Asset* loaded) {}

protected:
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
};

struct AssetLoaderRegistry
{
public:
    using Extension = std::string;
    using Creator = std::function<std::unique_ptr<AssetLoader>()>;
    static std::unique_ptr<AssetLoader> CreateAssetLoaderByExtension(const Extension& id);
    static std::unique_ptr<AssetLoader> CreateAssetLoaderByType(const std::type_info& type);
    static char RegisterAssetLoader(
        const std::vector<std::string>& exts, const Creator& creator, const std::vector<std::type_index>& types
    );

private:
    static std::unordered_map<Extension, std::function<std::unique_ptr<AssetLoader>()>>*
    GetAssetLoaderExtensionRegistry();

    static std::unordered_map<std::type_index, std::function<std::unique_ptr<AssetLoader>()>>*
    GetAssetLoaderTypeRegistry();
};

#define DECLARE_ASSET_LOADER()                                                                                         \
public:                                                                                                                \
    static const std::vector<std::string>& StaticGetExtensions();                                                      \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DEFINE_ASSET_LOADER(Type, Extension)                                                                           \
    char Type::_register = AssetLoaderRegistry::RegisterAssetLoader(                                                   \
        StaticGetExtensions(),                                                                                         \
        []() { return std::unique_ptr<AssetLoader>(new Type()); },                                                     \
        Type::GetImportTypes()                                                                                         \
    );                                                                                                                 \
    const std::vector<std::string>& Type::StaticGetExtensions()                                                        \
    {                                                                                                                  \
        static std::vector<std::string> extensions = GenerateExtensions(Extension, ',');                               \
        return extensions;                                                                                             \
    }
