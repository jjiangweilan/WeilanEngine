#pragma once
#include "Resource.hpp"
namespace Engine
{
std::unordered_map<AssetTypeID, std::function<std::unique_ptr<Resource>()>>* AssetRegister::GetRegisteredAsset()
{
    static std::unique_ptr<std::unordered_map<AssetTypeID, AssetRegister::Creator>> registeredAsset =
    std::make_unique<std::unordered_map<AssetTypeID, AssetRegister::Creator>>();
    return registeredAsset.get();
}

char AssetRegister::RegisterAsset(const AssetTypeID& assetID, const Creator& creator)
{
    GetRegisteredAsset()->emplace(assetID, creator);
    return '0';
}

std::unique_ptr<Resource> AssetRegister::CreateAsset(const AssetTypeID& id)
{
    auto iter = GetRegisteredAsset()->find(id);
    if (iter != GetRegisteredAsset()->end())
    {
        return iter->second();
    }

    return nullptr;
}


} // namespace Engine
