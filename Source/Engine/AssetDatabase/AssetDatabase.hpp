#pragma once
#include "Core/Asset.hpp"
#include "Internal/AssetData.hpp"
#include <filesystem>
namespace Engine
{
class AssetDatabase
{
public:
    AssetDatabase(const std::filesystem::path& projectRoot);

    void SaveDirtyAssets();

public:
    // path: relative path as projectRoot/Assets/{path}
    Asset* LoadAsset(std::filesystem::path path);
    Asset* LoadAssetByID(const UUID& uuid);

    Asset* SaveAsset(std::unique_ptr<Asset>&& asset, std::filesystem::path path);
    void SaveAsset(Asset& asset);

    bool IsAssetInDatabase(Asset& asset);

    void RequestShaderRefresh();
    void RefreshShader();

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

private:
    const std::filesystem::path projectRoot;
    const std::filesystem::path assetDirectory;
    const std::filesystem::path assetDatabaseDirectory;

    class Assets
    {
    public:
        Asset* Add(std::unique_ptr<AssetData>&& asset);

        // used for internal asset, internal asset needs to first Add to Assets but it doesn't have contained objects
        // yet, so after it loads it needs to update
        void UpdateAssetData(AssetData* assetData);
        AssetData* GetAssetData(const std::filesystem::path& path);

        // get by asset's uuid
        AssetData* GetAssetData(const UUID& uuid);

        std::unordered_map<std::filesystem::path, AssetData*> byPath;
        std::unordered_map<UUID, AssetData*> byUUID;
        std::vector<std::unique_ptr<AssetData>> data;
    } assets;

    SerializeReferenceResolveMap referenceResolveMap;
    std::vector<AssetData*> internalAssets;
    bool requestShaderRefresh;

    void SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path);
    void LoadEngineInternal();
};
} // namespace Engine
