#pragma once
#include "AssetData.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/AssetLoader.hpp"
#include "Engine/Core/Asset.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include <filesystem>

class AssetFileSystem
{
    std::unordered_map<AssetPath, AssetData*> byPath;
    std::unordered_map<UUID, AssetData*> byUUID;

    std::filesystem::path projectRoot;
    std::filesystem::path assetDirectory;
    std::filesystem::path assetDatabaseDirectory;

public:
    void Init(const std::filesystem::path& projectRoot);
    void SyncImportedAssetFiles(AssetData* assetData, const std::vector<AssetPath>& newImported);

    Asset* Add(AssetData* assetData);
    AssetData* GetAssetData(const AssetPath& path) const;
    AssetData* GetAssetData(const UUID& uuid) const;

    void Rename(const AssetPath& oldPath, const AssetPath& newPath);
    void Remove(const AssetPath& path);
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
