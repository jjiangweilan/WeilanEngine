#include "AssetImporter.hpp"

std::unique_ptr<AssetImporter> AssetImporterRegistry::CreateAssetImporterByExtension(const Extension& id)
{
    auto iter = GetAssetImporterExtensionRegistry()->find(id);
    if (iter != GetAssetImporterExtensionRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}
char AssetImporterRegistry::RegisterAssetImporter(
    const std::vector<std::string>& exts, const Creator& creator, const std::vector<std::type_index>& types
)
{
    for (auto& e : exts)
    {
        GetAssetImporterExtensionRegistry()->emplace(e, creator);
    }

    for (auto& t : types)
    {
        GetAssetImporterTypeRegistry()->emplace(t, creator);
    }
    return '0';
}

std::unordered_map<std::type_index, std::function<std::unique_ptr<AssetImporter>()>>* AssetImporterRegistry::
    GetAssetImporterTypeRegistry()
{
    static std::unordered_map<std::type_index, AssetImporterRegistry::Creator> registeredAssetImporter =
        std::unordered_map<std::type_index, AssetImporterRegistry::Creator>();
    return &registeredAssetImporter;
}

std::unordered_map<AssetImporterRegistry::Extension, std::function<std::unique_ptr<AssetImporter>()>>*
AssetImporterRegistry::GetAssetImporterExtensionRegistry()
{
    static std::unordered_map<Extension, AssetImporterRegistry::Creator> registeredAssetImporter =
        std::unordered_map<Extension, AssetImporterRegistry::Creator>();
    return &registeredAssetImporter;
}

std::unique_ptr<AssetImporter> AssetImporterRegistry::CreateAssetImporterByType(const std::type_info& type)
{
    auto iter = GetAssetImporterTypeRegistry()->find(type);
    if (iter != GetAssetImporterTypeRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}
