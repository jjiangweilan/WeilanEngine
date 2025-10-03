#pragma once
#include "AssetDatabase/Importers/AssetLoader.hpp"
#include "AssetDatabase/Private/AssetFileSystem.hpp"
#include "Libs/MPMCQueue.hpp"
#include "Libs/UUID.hpp"
#include <boost/unordered/concurrent_flat_map.hpp>

enum class AssetLoadingStatus
{
    Ready,
    Loading,
    NotLoaded
};

struct AsyncProcessedPayload
{
    AssetLoadingStatus loadingStatus;
    AssetData* assetData;
    Asset* asset;

    std::unique_ptr<Asset> loadedAsset;
    std::unique_ptr<AssetData> createdAssetData;

    AsyncProcessedPayload() = default;

    AsyncProcessedPayload(const AsyncProcessedPayload&) = delete;
    AsyncProcessedPayload(AsyncProcessedPayload&& other) noexcept
        : loadingStatus(other.loadingStatus), assetData(other.assetData), asset(other.asset),
          loadedAsset(std::move(other.loadedAsset)), createdAssetData(std::move(other.createdAssetData))
    {}

    AsyncProcessedPayload& operator=(const AsyncProcessedPayload& other) = delete;
    AsyncProcessedPayload& operator=(AsyncProcessedPayload&& other) noexcept
    {
        if (this != &other)
        {
            loadingStatus = other.loadingStatus;
            assetData = other.assetData;
            asset = other.asset;
            loadedAsset = std::move(other.loadedAsset);
            createdAssetData = std::move(other.createdAssetData);
        }

        return *this;
    }
};

class AsyncLoadProcessor
{
    ImportDatabase* importDatabase;
    AssetFileSystem* assetFileSystem;
    std::filesystem::path assetDirectory;
    boost::unordered::concurrent_flat_map<UUID, AsyncProcessedPayload> asyncProcessedPayload;

public:
    UUID AsyncLoadFromPath(const std::filesystem::path& path);

private:
    void AssetLoadingJob(AssetData* assetData, Asset* asset);

    std::unique_ptr<Asset> LoadAsset(const std::filesystem::path& path, const AssetMeta& assetMeta);
    std::unique_ptr<AssetData> CreateAssetData();
};
