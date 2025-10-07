#pragma once
#include "AssetDatabase/Importers/AssetImporter.hpp"
#include "AssetDatabase/Loaders/AssetLoader.hpp"
#include "AssetDatabase/Private/AssetFileSystem.hpp"
#include "Core/JobSystem.hpp"
#include "Libs/MPMCQueue.hpp"
#include "Libs/SpinLock.hpp"
#include "Libs/UUID.hpp"
#include <boost/lockfree/stack.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>

struct AsyncProcessedPayload
{
    AssetData* assetData;
    Asset* asset;

    std::unique_ptr<Asset> loadedAsset;

    AsyncProcessedPayload() = default;

    AsyncProcessedPayload(const AsyncProcessedPayload&) = delete;
    AsyncProcessedPayload(AsyncProcessedPayload&& other) noexcept
        : assetData(other.assetData), asset(other.asset), loadedAsset(std::move(other.loadedAsset))
    {}

    AsyncProcessedPayload& operator=(const AsyncProcessedPayload& other) = delete;
    AsyncProcessedPayload& operator=(AsyncProcessedPayload&& other) noexcept
    {
        if (this != &other)
        {
            assetData = other.assetData;
            asset = other.asset;
            loadedAsset = std::move(other.loadedAsset);
        }

        return *this;
    }
};

class AsyncLoadProcessor
{
    struct ScopedJobCounter
    {
        ScopedJobCounter(std::atomic_int& counter) : counter(counter) { counter++; }
        ~ScopedJobCounter() { counter--; }

        std::atomic_int& counter;
    };
    const ImportDatabase* importDatabase;
    const AssetFileSystem* assetFileSystem;
    std::filesystem::path assetDirectory;
    std::filesystem::path projectRoot;

    boost::unordered::concurrent_flat_map<UUID, AsyncProcessedPayload, std::hash<UUID>> asyncProcessedPayload;
    boost::unordered::concurrent_flat_map<std::filesystem::path, UUID> loadingAssets;

    std::atomic_int jobCounter = 0;

public:
    void Init(
        const ImportDatabase* importDatabase,
        const AssetFileSystem* assetFileSystem,
        const std::filesystem::path& assetDirectory,
        const std::filesystem::path& projectRoot
    )
    {
        this->importDatabase = importDatabase;
        this->assetFileSystem = assetFileSystem;
        this->assetDirectory = assetDirectory;
        this->projectRoot = projectRoot;
    }

    void PollAsyncLoading();
    ObjPtr<Asset> AsyncLoadFromPath(const std::filesystem::path& path);
    std::unique_ptr<Asset> LoadAssetJob(const std::filesystem::path& path, AssetData* assetData);
    void SyncLoad();

private:
};
