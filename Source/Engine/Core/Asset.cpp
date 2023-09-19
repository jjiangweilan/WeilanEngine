#include "Asset.hpp"
namespace Engine
{
std::unordered_map<AssetTypeID, std::function<std::unique_ptr<Asset>()>>* AssetRegistry::GetAssetTypeRegistery()
{
    static std::unique_ptr<std::unordered_map<AssetTypeID, AssetRegistry::Creator>> registeredAsset =
        std::make_unique<std::unordered_map<AssetTypeID, AssetRegistry::Creator>>();
    return registeredAsset.get();
}

char AssetRegistry::RegisterAsset(const AssetTypeID& assetID, const char* ext, const Creator& creator)
{
    GetAssetTypeRegistery()->emplace(assetID, creator);
    GetAssetExtensionRegistry()->emplace(std::string(ext), creator);
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

std::unordered_map<AssetRegistry::Extension, std::function<std::unique_ptr<Asset>()>>* AssetRegistry::
    GetAssetExtensionRegistry()
{

    static std::unique_ptr<std::unordered_map<Extension, AssetRegistry::Creator>> registeredAsset =
        std::make_unique<std::unordered_map<Extension, AssetRegistry::Creator>>();
    return registeredAsset.get();
}

std::unique_ptr<Asset> AssetRegistry::CreateAsset(const AssetTypeID& id)
{
    auto iter = GetAssetTypeRegistery()->find(id);
    if (iter != GetAssetTypeRegistery()->end())
    {
        return iter->second();
    }

    return nullptr;
}

} // namespace Engine
