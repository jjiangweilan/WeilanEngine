#pragma once
#include "Engine/Core/Asset.hpp"
#include "Engine/Library/Serialization/BinarySerializer.hpp"
#include "Engine/Library/Serialization/JsonSerializer.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include <filesystem>
#include <fstream>
#include <memory>

// perf: consider change this with a Copy-On-Write wrap, copying json is slow
using AssetMeta = nlohmann::json;

// an AssetData represent an imported Asset, it contains meta information about the asset and the asset itself
class AssetData
{
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
    AssetPath assetPath = {};
    std::filesystem::path absolutePath = {};

    std::vector<AssetPath> importedAssetFilePaths = {};

    nlohmann::json meta = nlohmann::json::object();

    bool isValid = false;

    std::unique_ptr<Asset> asset = nullptr;

    std::unordered_map<std::string, UUID> nameToUUID = {};

    bool dirty = false;
    bool internal = false;

    friend class AssetFileSystem;

public:
    struct InternalAssetDataTag
    {};

    AssetData();

    // this is used when saving an Asset
    AssetData(
        std::unique_ptr<Asset>&& resource,
        const AssetPath& assetPath,
        const std::filesystem::path& projectRoot
    );

    // this is used when loading an Asset
    //
    AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot);

    // used for internal Asset
    AssetData(const UUID& assetUUID, const AssetPath& internalAssetPath, InternalAssetDataTag);

    // used for new asset (just import)
    AssetData(const AssetPath& assetPath, const std::filesystem::path& projectRoot);

    // used for async unimported asset that needs
    ~AssetData();

    const UUID& GetAssetUUID() const { return assetUUID; }

    const UUID& GetAssetDataUUID() const { return assetDataUUID; }

    // used to check if construction of AssetData is valid
    bool IsValid() const { return isValid; }

    // if the file on disk's write time is newer than the one recorded
    bool NeedRefresh() const;
    void UpdateLastWriteTime();

    void SetAssetPath(const AssetPath& path)
    {
        assetPath = path;
        absolutePath = assetPath.ToAbsolutePath();
        internal = assetPath.IsInternal();
    }

    std::string GetNameToUUIDKey(Asset* obj);

    const AssetPath& GetAssetPath() { return assetPath; };
    const std::filesystem::path& GetAssetAbsolutePath() { return absolutePath; }
    void UpdateAssetUUIDs();
    Asset* SetAsset(std::unique_ptr<Asset>&& asset, const std::filesystem::path& projectRoot);
    Asset* GetAsset();
    void UnloadAsset() { asset = nullptr; }

    std::unordered_map<std::string, UUID>& GetInternalObjectAssetNameToUUID() { return nameToUUID; }

    bool IsDirty() { return dirty; }

    nlohmann::json DumpInfo() const;

    void SaveToDisk(const std::filesystem::path& projectRoot);

    bool ReimportNeeded() { return false; }

    void SetMeta(const nlohmann::json& meta)
    {
        dirty = true;
        this->meta = meta;
    }

    void SetImportedAssetPaths(const std::vector<AssetPath>& paths) { importedAssetFilePaths = paths; }

    std::vector<AssetPath> GetImportedAssetPaths() { return importedAssetFilePaths; }

    const nlohmann::json& GetMeta() { return meta; }
};
