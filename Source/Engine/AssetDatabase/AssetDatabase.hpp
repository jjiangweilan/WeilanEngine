#pragma once
#include "Asset.hpp"
#include "Core/Resource.hpp"
#include <filesystem>

namespace Engine
{

class AssetDatabase
{
public:
    AssetDatabase(const std::filesystem::path& path) { assetRootPath = path; };

public:
    Asset* LoadAsset(const std::filesystem::path& path);

    template <class T>
    Asset* CreateAsset(std::unique_ptr<T>&& resource, const std::filesystem::path& path);

private:
    std::filesystem::path assetRootPath;
    std::unordered_map<std::string, std::shared_ptr<Asset>> assets;
};

template <class T>
Asset* AssetDatabase::CreateAsset(std::unique_ptr<T>&& resource, const std::filesystem::path& path)
{
    std::shared_ptr<Asset> newAsset = std::make_unique<Asset>(std::move(resource), assetRootPath / path);

    newAsset->Save<T>();

    Asset* temp = newAsset.get();
    assets[path.string()] = std::move(newAsset);

    return temp;
}
} // namespace Engine
