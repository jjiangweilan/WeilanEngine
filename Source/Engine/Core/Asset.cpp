#include "Asset.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include <fstream>

bool Asset::SaveToFile(const std::filesystem::path& path) const
{
    JsonSerializer serializer;
    Serialize(&serializer);
    std::vector<uint8_t> data = serializer.GetBinary();

    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open() || !output.good())
        return false;

    if (!data.empty())
        output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    return output.good();
}

char AssetRegistry::RegisterAsset(
    const ObjectTypeID& assetID, const std::vector<std::string>& exts, const Creator& creator
)
{
    GetAssetTypeRegistery()->emplace(assetID, creator);
    for (auto& e : exts)
    {
        GetAssetExtensionRegistry()->emplace(e, creator);
    }
    return '0';
}

std::unique_ptr<Asset> AssetRegistry::CreateAssetByExtension(const Extension& id)
{
    auto iter = GetAssetExtensionRegistry()->find(id);
    if (iter != GetAssetExtensionRegistry()->end())
    {
        return iter->second();
    }

    return nullptr;
}

bool AssetRegistry::IsExtensionAnAsset(const std::string& ext)
{
    auto registery = GetAssetExtensionRegistry();
    return registery->find(ext) != registery->end();
}

std::unordered_map<AssetRegistry::Extension, std::function<std::unique_ptr<Asset>()>>* AssetRegistry::
    GetAssetExtensionRegistry()
{

    static std::unordered_map<Extension, AssetRegistry::Creator> registeredAsset =
        std::unordered_map<Extension, AssetRegistry::Creator>();
    return &registeredAsset;
}

std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Asset>()>>* AssetRegistry::GetAssetTypeRegistery()
{
    static std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Asset>()>> registeredAsset =
        std::unordered_map<ObjectTypeID, std::function<std::unique_ptr<Asset>()>>();
    return &registeredAsset;
}

std::unique_ptr<Asset> AssetRegistry::CreateAsset(const ObjectTypeID& id)
{
    auto iter = GetAssetTypeRegistery()->find(id);
    if (iter != GetAssetTypeRegistery()->end())
    {
        return iter->second();
    }

    return nullptr;
}
