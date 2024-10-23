#pragma once
#include "Core/Asset.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <typeindex>

class ImportDatabase
{
public:
    void Init(const std::filesystem::path& importDatabaseRoot)
    {
        this->importDatabaseRoot = importDatabaseRoot;
    }
    std::vector<uint8_t> ReadFile(const std::string& filename);

    std::filesystem::path GetImportAssetPath(const std::string& filename);

private:
    std::filesystem::path importDatabaseRoot;
};

class AssetLoader
{
public:
    virtual void Setup(ImportDatabase& importDatabase, const std::filesystem::path& assetPath, nlohmann::json& meta)
    {
        absoluteAssetPath = assetPath;
        this->meta = meta;
        this->importDatabase = &importDatabase;
    }

    virtual bool ImportNeeded() = 0;
    virtual void Import() = 0;
    virtual void Load() = 0;

    // no need to override if this data import doesn't need reference resolving
    virtual void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap)
    {
        serializer = nullptr;
        resolveMap = nullptr;
    }
    virtual std::unique_ptr<Asset> RetrieveAsset() = 0;
    virtual nlohmann::json GetMeta()
    {
        return meta;
    }

protected:
    // asset to import
    std::filesystem::path absoluteAssetPath{};

    // meta in the AssetDatabase
    nlohmann::json meta;
    ImportDatabase* importDatabase;

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
