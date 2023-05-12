#pragma once
#include "Resource.hpp"
namespace Engine
{
char AssetRegister::RegisterAsset(const AssetID& assetID, const Creator& creator)
{
    registeredAsset[assetID] = creator;
    return '0';
}

std::unordered_map<AssetID, AssetRegister::Creator> AssetRegister::registeredAsset =
    std::unordered_map<AssetID, AssetRegister::Creator>();
} // namespace Engine
