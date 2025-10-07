#include "AsyncLoadProcessor.hpp"
#include "Core/JobSystem.hpp"

ObjPtr<Asset> AsyncLoadProcessor::AsyncLoadFromPath(const std::filesystem::path& path)
{
    ScopedJobCounter _c(jobCounter);

    AssetData* assetData = assetFileSystem->GetAssetData(path);
    UUID ret = UUID::GetEmptyUUID();

    if (!loadingAssets.try_emplace_or_visit(path, assetData->GetAssetUUID(), [&ret](auto& val) { ret = val.second; }))
    {
        return ObjPtr<Asset>(ret);
    }

    std::filesystem::path ext = path.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());

    // if this asset is already loaded, we can just try its UUID
    ASSERT(assetData != nullptr);

    Asset* asset;
    if (assetData)
        asset = assetData->GetAsset();

    // Case: asset is already loaded
    if (asset)
    {
        ret = asset->GetUUID();
    }
    // Case: has assetData but the actual asset is not loaded
    else
    {
        ret = assetData->GetAssetUUID();

        jobCounter++;
        auto job = [this, ret, path, assetData]()
        {
            auto loadedAsset = LoadAssetJob(path, assetData);

            AsyncProcessedPayload payload{};
            payload.assetData = assetData;
            payload.asset = loadedAsset.get();
            payload.loadedAsset = std::move(loadedAsset);

            asyncProcessedPayload.emplace(ret, std::move(payload));
            jobCounter--;
        };

        JobSystem::Instance().Schedule(std::move(job));
    }

    return ObjPtr<Asset>(ret);
}

std::unique_ptr<Asset> AsyncLoadProcessor::LoadAssetJob(const std::filesystem::path& path, AssetData* assetData)
{
    // copy json meta is slow, so we use pointer here
    static nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json* assetMeta = &empty;

    // use path relative to AssetDirectory
    if (path.is_absolute())
        return nullptr;

    // find the asset if it's already imported
    auto absoluteAssetPath = assetData->GetAssetAbsolutePath();

    if (!std::filesystem::exists(absoluteAssetPath))
        return nullptr;

    // this asset is already imported once, we can read its meta
    ASSERT(assetData != nullptr);
    assetMeta = &assetData->GetMeta();

    // override the asset path because this asset may be an internal asset
    absoluteAssetPath = assetData->GetAssetAbsolutePath();

    std::filesystem::path ext = absoluteAssetPath.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());
    if (loader == nullptr)
        return nullptr;

    loader->Setup(importDatabase, absoluteAssetPath, *assetMeta);

    Asset* asset = assetData ? assetData->GetAsset() : nullptr;
    bool isReload = asset == nullptr;

    loader->Load();
    std::unique_ptr<Asset> newAsset = loader->RetrieveAsset();

    // failed to load asset
    if (newAsset == nullptr)
    {
        return nullptr;
    }

    asset = newAsset.get();

    // newly imported or loaded, resolve references
    Serializer* serializer;
    SerializeReferenceResolveMap* localResolveMap;
    loader->GetReferenceResolveData(serializer, localResolveMap);

    if (serializer)
    {
        for (auto& uuid : serializer->GetReferencedObjects())
        {
            auto assetData = assetFileSystem->GetAssetData(uuid);
            if (assetData)
            {
                AsyncLoadFromPath(assetData->GetAssetPath());
            }
        }
    }

    // asset->OnLoaded();

    return newAsset;
}

void AsyncLoadProcessor::SyncLoad()
{
    if (std::this_thread::get_id() == JobSystem::Instance().GetMainThreadID())
    {
        while (jobCounter != 0)
            std::this_thread::yield();

        PollAsyncLoading();
    }
}

void AsyncLoadProcessor::PollAsyncLoading()
{
    ASSERT(std::this_thread::get_id() == JobSystem::Instance().GetMainThreadID());

    std::vector<std::pair<UUID, AssetData*>> finishedJobs{};

    asyncProcessedPayload.visit_all(
        [projectRoot = this->projectRoot, &finishedJobs](std::pair<const UUID, AsyncProcessedPayload>& payload)
        {
            if (payload.second.loadedAsset)
            {
                auto asset = payload.second.assetData->SetAsset(std::move(payload.second.loadedAsset), projectRoot);
            }

            finishedJobs.emplace_back(payload.first, payload.second.assetData);
        }
    );

    for (auto& finished : finishedJobs)
    {
        if (auto asset = finished.second->GetAsset())
        {
            asset->OnLoaded();
        }
    }

    for (auto& finished : finishedJobs)
    {
        asyncProcessedPayload.erase(finished.first);
        loadingAssets.erase(finished.second->GetAssetPath());
    }
}
