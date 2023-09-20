#include "Asset.hpp"
namespace Engine
{
char AssetRegistry::RegisterAsset(const ObjectTypeID& assetID, const char* ext, const Creator& creator)
{
    ObjectRegistry::RegisterObject(assetID, creator);
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

std::unique_ptr<Asset> AssetRegistry::CreateAsset(const ObjectTypeID& id)
{
    auto iter = GetAssetTypeRegistery()->find(id);
    if (iter != GetAssetTypeRegistery()->end())
    {
        return iter->second();
    }

    return nullptr;
}

} // namespace Engine
