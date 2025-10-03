#include "AsyncLoadProcessor.hpp"
#include "Core/JobSystem.hpp"

UUID AsyncLoadProcessor::AsyncLoadFromPath(const std::filesystem::path& path)
{
    UUID ret = UUID::GetEmptyUUID();

    auto absoluteAssetPath = assetDirectory / path;

    std::filesystem::path ext = absoluteAssetPath.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());

    // if this asset is already loaded, we can just try its UUID
    AssetData* assetData = assetFileSystem->GetAssetData(path);
    Asset* asset;
    if (assetData)
        asset = assetData->GetAsset();

    // Case: asset is already loaded
    if (assetData && asset)
    {
        ret = asset->GetUUID();

        AsyncProcessedPayload payload{};
        payload.loadingStatus = AssetLoadingStatus::Ready;
        payload.assetData = assetData;
        payload.asset = asset;

        asyncProcessedPayload.emplace(ret, payload);
    }
    // Case: has assetData but the actual asset is not loaded
    else if (assetData && !asset)
    {
        ret = assetData->GetAssetUUID();

        auto job = [this, ret, path, assetData]()
        {
            auto loadedAsset = LoadAsset(path, assetData);

            AsyncProcessedPayload payload{};
            payload.loadingStatus = AssetLoadingStatus::Loading;
            payload.assetData = assetData;
            payload.asset = loadedAsset.get();
            payload.loadedAsset = std::move(loadedAsset);

            asyncProcessedPayload.emplace(ret, std::move(payload));
        };

        JobSystem::Instance().Schedule(std::move(job));
    }
    // Case: no assetData and no asset
    else
    {
        std::unique_ptr<AssetData> createdAssetData = CreateAssetData();
        ret = createdAssetData->GetAssetUUID();

        auto job =
            [this, ret, path, createdAssetData = std::unique_ptr<AssetData>(std::move(createdAssetData))]() mutable
        {
            auto loadedAsset = LoadAsset(path, createdAssetData.get());

            AsyncProcessedPayload payload{};
            payload.loadingStatus = AssetLoadingStatus::Loading;
            payload.assetData = createdAssetData.get();
            payload.asset = loadedAsset.get();
            payload.loadedAsset = std::move(loadedAsset);
            payload.createdAssetData = std::move(createdAssetData);

            asyncProcessedPayload.emplace(ret, std::move(payload));
        };

        JobSystem::Instance().Schedule(std::move(job));
    }

    return ret;
}

std::unique_ptr<Asset> AsyncLoadProcessor::LoadAsset(const std::filesystem::path& path, AssetData* assetData)
{
    const AssetMeta& assetMeta = assetData->GetMeta();
    auto ext = path.extension();
    auto absoluteAssetPath = assetDirectory / path;

    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());
    if (loader == nullptr)
        return nullptr;

    loader->Setup(*importDatabase, absoluteAssetPath, assetMeta);

    bool importNeeded = loader->ImportNeeded();
    std::vector<std::filesystem::path> importedAssetFilePaths;
    if (importNeeded)
    {
        importedAssetFilePaths = loader->Import();

        if (assetData != nullptr)
        {
            assetFileSystem.SyncImportedAssetFiles(assetData, importedAssetFilePaths);
        }
    }
}
std::unique_ptr<AssetData> AsyncLoadProcessor::CreateAssetData() {}
