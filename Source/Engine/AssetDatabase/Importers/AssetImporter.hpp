#pragma once
#include <nlohmann/json.hpp>

class AssetImporter
{
public:
    virtual void Import(nlohmann::json& meta) = 0;

private:
};

struct AssetImporterRegistry
{
public:
    using Extension = std::string;
    using Creator = std::function<std::unique_ptr<AssetImporter>()>;
    static std::unique_ptr<AssetImporter> CreateAssetImporterByExtension(const Extension& id);
    static char RegisterAssetImporter(const std::vector<std::string>& exts, const Creator& creator);

private:
    static std::unordered_map<Extension, std::function<std::unique_ptr<AssetImporter>()>>*
    GetAssetImporterExtensionRegistry();
};

#define DECLARE_ASSET_IMPORTER()                                                                                       \
public:                                                                                                                \
    static const std::vector<std::string>& GetExtensions();                                                      \
                                                                                                                       \
private:                                                                                                               \
    static char _register;
