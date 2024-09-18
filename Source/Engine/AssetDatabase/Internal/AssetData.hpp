#pragma once
#include "Core/Asset.hpp"
#include "Libs/Serialization/BinarySerializer.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

// an AssetData represent an imported Asset, it contains meta information about the asset and the asset itself
class AssetData
{
public:
    struct InternalAssetDataTag
    {};
    // this is used when saving an Asset
    AssetData(
        std::unique_ptr<Asset>&& resource,
        const std::filesystem::path& assetPath,
        const std::filesystem::path& projectRoot
    );

    // this is used when loading an Asset
    //
    AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot);

    // used for internal Asset
    AssetData(const UUID& assetUUID, const std::filesystem::path& internalAssetPath, InternalAssetDataTag);
    ~AssetData();

    const UUID& GetAssetUUID() const
    {
        return assetUUID;
    }

    // used to check if construction of AssetData is valid
    bool IsValid() const
    {
        return isValid;
    }

    // if the file on disk's write time is newer than the one recorded
    bool NeedRefresh() const;
    void UpdateLastWriteTime();

    const std::filesystem::path& GetAssetPath()
    {
        return assetPath;
    };

    const std::filesystem::path& GetAssetAbsolutePath()
    {
        return absolutePath;
    }

    bool ChangeAssetPath(const std::filesystem::path& path, const std::filesystem::path& projectRoot);

    void UpdateAssetUUIDs();
    Asset* SetAsset(std::unique_ptr<Asset>&& asset);
    Asset* GetAsset();

    std::unordered_map<std::string, UUID>& GetInternalObjectAssetNameToUUID()
    {
        return nameToUUID;
    }

    bool IsDirty()
    {
        return dirty;
    }

    void SaveToDisk(const std::filesystem::path& projectRoot);

    bool ReimportNeeded()
    {
        return false;
    }

private:
    // scaii code stands for Wei Lan Engine AssetFile
    static const uint32_t WLEA = 0b01010111 << 24 | 0b01001100 << 16 | 0b01000101 << 8 | 0b01000001;

    // each AssetData has an uuid
    UUID assetDataUUID;

    // the uuid of the asset this assetData linked to
    UUID assetUUID;

    // the resource's type id
    ObjectTypeID assetTypeID;

    long long lastWriteTime;

    // this is the path to the resource the AssetFile linked to
    // relative path in Assets/
    // if it's an engine internal file it be _engine_internal/xxx
    std::filesystem::path assetPath;
    std::filesystem::path absolutePath;

    bool isValid = false;

    std::unique_ptr<Asset> asset;

    std::unordered_map<std::string, UUID> nameToUUID;

    bool dirty = false;
    bool internal = false;

    friend class AssetDatabase;
};
