#pragma once
#include "Core/Asset.hpp"
#include "Internal/AssetData.hpp"
#include <filesystem>
class AssetDatabase
{
public:
    AssetDatabase(const std::filesystem::path& projectRoot);

    void SaveDirtyAssets();

    // path: relative path as projectRoot/Assets/{path}
    Asset* LoadAsset(std::filesystem::path path);
    Asset* LoadAssetByID(const UUID& uuid);

    std::vector<Asset*> LoadAssets(std::span<std::filesystem::path> pathes);

    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);
    void SaveAsset(Asset& asset);

    bool IsAssetInDatabase(Asset& asset);

    void RequestShaderRefresh(bool all = false);
    void RefreshShader();
    bool ChangeAssetPath(const std::filesystem::path& src, const std::filesystem::path& dst);

    const std::filesystem::path& GetAssetDirectory()
    {
        return assetDirectory;
    }

    const std::filesystem::path& GetAssetDirectory() const
    {
        return assetDirectory;
    }

    const std::vector<AssetData*>& GetInternalAssets() const
    {
        return internalAssets;
    }

    static AssetDatabase* Singleton()
    {
        return SingletonReference();
    }

private:
    static AssetDatabase*& SingletonReference();
    const std::filesystem::path projectRoot;
    const std::filesystem::path assetDirectory;
    const std::filesystem::path assetDatabaseDirectory;

    class Assets
    {
    public:
        struct PathHasher
        {
            size_t operator()(const std::filesystem::path& path) const
            {
                return std::filesystem::hash_value(path);
            }
        };
        Asset* Add(std::unique_ptr<AssetData>&& asset);

        // used for internal asset, internal asset needs to first Add to Assets but it doesn't have contained objects
        // yet, so after it loads it needs to update
        void UpdateAssetData(AssetData* assetData);
        AssetData* GetAssetData(const std::filesystem::path& path);

        // get by asset's uuid
        AssetData* GetAssetData(const UUID& uuid);

        std::unordered_map<std::filesystem::path, AssetData*, PathHasher> byPath;
        std::unordered_map<UUID, AssetData*> byUUID;
        std::vector<std::unique_ptr<AssetData>> data;
    } assets;

    SerializeReferenceResolveMap referenceResolveMap;
    std::unordered_map<UUID, int*> managedObjectCounters;

    std::vector<AssetData*> internalAssets;
    bool requestShaderRefresh = false;
    bool requestShaderRefreshAll = false;

    void SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path);
    void LoadEngineInternal();

    // used to set instance
    friend class WeilanEngine;
};
