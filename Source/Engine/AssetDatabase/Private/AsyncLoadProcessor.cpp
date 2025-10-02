#include "AsyncLoadProcessor.hpp"

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

        auto loadedAsset = LoadAsset(path);

        AsyncProcessedPayload payload{};
        payload.loadingStatus = AssetLoadingStatus::Loading;
        payload.assetData = assetData;
        payload.asset = loadedAsset.get();
        payload.loadedAsset = std::move(loadedAsset);

        asyncProcessedPayload.emplace(ret, std::move(payload));
    }
    // Case: no assetData and no asset
    else
    {
        auto createdAssetData = CreateAssetData();
        auto loadedAsset = LoadAsset(path);

        AsyncProcessedPayload payload{};
        payload.loadingStatus = AssetLoadingStatus::Loading;
        payload.assetData = assetData;
        payload.asset = loadedAsset.get();
        payload.loadedAsset = std::move(loadedAsset);
        payload.createdAssetData = std::move(createdAssetData);

        asyncProcessedPayload.emplace(ret, std::move(payload));
    }

    return ret;
}

std::unique_ptr<Asset> AsyncLoadProcessor::LoadAsset(const std::filesystem::path& path) {}
std::unique_ptr<AssetData> AsyncLoadProcessor::CreateAssetData() {}
