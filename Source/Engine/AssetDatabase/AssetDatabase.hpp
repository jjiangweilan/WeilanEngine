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

    template <IsResource T>
    Asset* SaveAsset(std::unique_ptr<T>&& resource, const std::filesystem::path& path);

private:
    std::filesystem::path assetRootPath;
};

template <IsResource T>
Asset* AssetDatabase::SaveAsset(std::unique_ptr<T>&& resource, const std::filesystem::path& path)
{
    AssetImpl<T> newAsset(std::move(resource), path);

    newAsset.Save();
}
} // namespace Engine
