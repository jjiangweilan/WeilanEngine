#pragma once
#include "AssetDatabase/Private/ImportDatabase.hpp"
#include "Libs/PodVector.hpp"
#include "Libs/Utils.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
#include <typeindex>
#include <vector>

class AssetImporter
{
protected:
    // asset to import
    //
    std::filesystem::path absoluteAssetPath{};

    // meta in the AssetDatabase
    nlohmann::json meta;
    const ImportDatabase* importDatabase;

public:
    void Setup(const ImportDatabase& importDatabase, const std::filesystem::path& assetPath, const nlohmann::json& meta)
    {
        this->absoluteAssetPath = assetPath;
        this->importDatabase = &importDatabase;
        this->meta = meta;
    }
    virtual ~AssetImporter() {}
    virtual nlohmann::json GetMeta() { return meta; }
    virtual std::vector<std::filesystem::path> Import() = 0;
    virtual bool ImportNeeded() = 0;
    virtual bool IsInternalAsset() { return false; }

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
};

struct AssetImporterRegistry
{
public:
    using Extension = std::string;
    using Creator = std::function<std::unique_ptr<AssetImporter>()>;
    static std::unique_ptr<AssetImporter> CreateAssetImporterByExtension(const Extension& id);
    static std::unique_ptr<AssetImporter> CreateAssetImporterByType(const std::type_info& type);
    static char RegisterAssetImporter(
        const std::vector<std::string>& exts, const Creator& creator, const std::vector<std::type_index>& types
    );

private:
    static std::unordered_map<Extension, std::function<std::unique_ptr<AssetImporter>()>>*
    GetAssetImporterExtensionRegistry();

    static std::unordered_map<std::type_index, std::function<std::unique_ptr<AssetImporter>()>>*
    GetAssetImporterTypeRegistry();
};

#define DECLARE_ASSET_IMPORTER()                                                                                       \
public:                                                                                                                \
    static const std::vector<std::string>& StaticGetExtensions();                                                      \
                                                                                                                       \
private:                                                                                                               \
    static char _register;

#define DEFINE_ASSET_IMPORTER(Type, Extension)                                                                         \
    char Type::_register = AssetImporterRegistry::RegisterAssetImporter(                                               \
        StaticGetExtensions(),                                                                                         \
        []() { return std::unique_ptr<AssetImporter>(new Type()); },                                                   \
        Type::GetImportTypes()                                                                                         \
    );                                                                                                                 \
    const std::vector<std::string>& Type::StaticGetExtensions()                                                        \
    {                                                                                                                  \
        static std::vector<std::string> extensions = GenerateExtensions(Extension, ',');                               \
        return extensions;                                                                                             \
    }
