#pragma once
#include "Core/Asset.hpp"
#include "Libs/Serialization/BinarySerializer.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

namespace Engine
{
// an AssetData represent an imported Asset, it contains meta information about the asset and the asset itself
class AssetData
{
public:
    // this is used when saving an AssetFile
    AssetData(
        std::unique_ptr<Asset>&& resource,
        const std::filesystem::path& assetPath,
        const std::filesystem::path& projectRoot
    );

    // this is used when loading an AssetFile
    //
    AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot);

    const UUID& GetUUID() const;

    // used to check if construction of AssetData is valid
    bool IsValid() const
    {
        return isValid;
    }

    const std::filesystem::path& GetAssetPath()
    {
        return assetPath;
    };

    Asset* GetAsset();

private:
    // scaii code stands for Wei Lan Engine AssetFile
    static const uint32_t WLEA = 0b01010111 << 24 | 0b01001100 << 16 | 0b01000101 << 8 | 0b01000001;

    // each AssetData has an uuid
    UUID assetDataUUID;

    // the uuid of the asset this assetData linked to
    UUID assetUUID;

    // the resource's type id
    AssetTypeID assetTypeID;

    // this is the path to the resource the AssetFile linked to
    // relative path in Assets/
    std::filesystem::path assetPath;

    bool isValid = false;

    std::unique_ptr<Asset> asset;
};
} // namespace Engine
