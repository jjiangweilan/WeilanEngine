#pragma once
#include "AssetData.hpp"
#include "Runtime/System/AssetDatabase/Loaders/AssetLoader.hpp"
#include "Core/Asset.hpp"
#include <filesystem>

class AssetFileSystem
{
    struct PathHasher
    {
        size_t operator()(const std::filesystem::path& path) const { return std::filesystem::hash_value(path); }
    };

    std::unordered_map<std::filesystem::path, AssetData*, PathHasher> byPath;
    std::unordered_map<UUID, AssetData*> byUUID;

    std::filesystem::path projectRoot;
    std::filesystem::path assetDirectory;
    std::filesystem::path assetDatabaseDirectory;

public:
    void Init(const std::filesystem::path& projectRoot);
    void SyncImportedAssetFiles(AssetData* assetData, const std::vector<std::filesystem::path>& newImported);

    Asset* Add(AssetData* assetData);
    AssetData* GetAssetData(const std::filesystem::path& path) const;
    AssetData* GetAssetData(const UUID& uuid) const;

    void Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
    void Remove(const std::filesystem::path& path);
    void RemoveAssetData(AssetData* assetData);
    void UnloadAsset(Asset& asset);

private:
    /**
     used for internal asset, internal asset needs to first Add to Assets but it doesn't have contained objects yet, so
     after it loads it needs to update
    */
    void UpdateAssetData(AssetData* assetData);

    const std::filesystem::path& GetAssetDirectory() const { return assetDirectory; }
    const std::filesystem::path& GetProjectRoot() const { return projectRoot; }
    const std::filesystem::path& GetProjectAssetDatabaseDirectory() const { return assetDatabaseDirectory; }
};
