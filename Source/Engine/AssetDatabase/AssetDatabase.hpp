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

    JsonSerializer ser;
    newAsset->Serialize<T>(&ser);

    std::ofstream out;
    out.open(newAsset->GetPath(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    if (out.is_open() && out.good())
    {
        auto binary = ser.GetBinary();

        if (binary.size() != 0)
        {
            out.write((char*)binary.data(), binary.size());
        }
    }
    out.close();

    Asset* temp = newAsset.get();
    assets[path.string()] = std::move(newAsset);

    return temp;
}
} // namespace Engine
