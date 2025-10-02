#pragma once
#include "Core/Asset.hpp"
#include "Libs/PodVector.hpp"
#include "Libs/Serialization/Serializer.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <typeindex>

class ImportDatabase
{
public:
    void Init(const std::filesystem::path& importDatabaseRoot) { this->importDatabaseRoot = importDatabaseRoot; }
    PodVector<uint8_t> ReadFile(const std::string& filename);

    std::filesystem::path GetImportAssetPath(const std::string& filename);

private:
    const size_t streamBufSize = 1024 * 1024;
    PodVector<char> streamBuf = PodVector<char>(streamBufSize);
    std::filesystem::path importDatabaseRoot;
};

class AssetLoader
{
protected:
    // asset to import
    std::filesystem::path absoluteAssetPath{};

    // meta in the AssetDatabase
    nlohmann::json meta;
    ImportDatabase* importDatabase;
    std::vector<std::unique_ptr<AssetLoader>> dependencies;

public:
    virtual ~AssetLoader() {}
    virtual void Setup(
        ImportDatabase& importDatabase, const std::filesystem::path& assetPath, const nlohmann::json& meta
    )
    {
        this->absoluteAssetPath = assetPath;
        this->meta = meta;
        this->importDatabase = &importDatabase;
    }

    virtual bool IsInternalAsset() { return false; }
    virtual bool ImportNeeded() = 0;

    // imported file path
    virtual std::vector<std::filesystem::path> Import() = 0;
    virtual void Load() = 0;

    // no need to override if this data import doesn't need reference resolving
    virtual void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap)
    {
        serializer = nullptr;
        resolveMap = nullptr;
    }
    virtual std::unique_ptr<Asset> RetrieveAsset() = 0;
    virtual nlohmann::json GetMeta() { return meta; }

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
