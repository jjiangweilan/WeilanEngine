#include "AsyncLoadProcessor.hpp"
#include "Engine/Core/JobSystem.hpp"

std::unique_ptr<Asset> AsyncLoadProcessor::ExecuteLoader(std::unique_ptr<AssetLoader>& loader, AssetData* assetData, bool async)
{
    auto assetMeta = &assetData->GetMeta();
    const auto& absoluteAssetPath = assetData->GetAssetAbsolutePath();
    loader->Setup(importDatabase, assetData->GetAssetUUID(), absoluteAssetPath, *assetMeta);

    loader->Load();
    std::unique_ptr<Asset> newAsset = loader->RetrieveAsset();

    // failed to load asset
    if (newAsset == nullptr)
    {
        return nullptr;
    }

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
                DispatchLoadJobFromPath(assetData->GetAssetPath(), async);
            }
        }
    }

    return newAsset;
}

ObjPtr<Asset> AsyncLoadProcessor::DispatchLoadJobFromPath(const AssetPath& path, bool async)
{
    ScopedJobCounter _c(jobCounter);

    AssetData* assetData = assetFileSystem->GetAssetData(path);

    if (assetData == nullptr)
        return nullptr;

    UUID ret = UUID::GetEmptyUUID();

    if (!loadingAssets.try_emplace_or_visit(path, assetData->GetAssetUUID(), [&ret](auto& val)
                                            { ret = val.second; }))
    {
        return ObjPtr<Asset>(ret);
    }

    std::filesystem::path ext = path.ToFilesystemPath().extension();
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
            AsyncProcessedPayload payload{};

            auto loadedAsset = LoadAssetJob(path, assetData, payload.requireMainThreadLoading, payload.loader);

            payload.assetData = assetData;
            payload.asset = loadedAsset.get();
            payload.loadedAsset = std::move(loadedAsset);

            asyncProcessedPayload.emplace(ret, std::move(payload));
            jobCounter--;
        };

        if (async)
        {
            JobSystem::Instance().Schedule(std::move(job));
        }
        else
        {
            job();
        }
    }

    return ObjPtr<Asset>(ret);
}

std::unique_ptr<Asset> AsyncLoadProcessor::LoadAssetJob(const AssetPath& path, AssetData* assetData, bool& requireMainThreadLoading, std::unique_ptr<AssetLoader>& outLoader)
{
    outLoader = nullptr;
    requireMainThreadLoading = false;

    // find the asset if it's already imported
    const auto& absoluteAssetPath = assetData->GetAssetAbsolutePath();

    if (!std::filesystem::exists(absoluteAssetPath))
        return nullptr;

    // this asset is already imported once, we can read its meta
    ASSERT(assetData != nullptr);

    std::filesystem::path ext = absoluteAssetPath.extension();
    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext.string());

    if (loader == nullptr)
        return nullptr;

    if (!loader->SupportsAsyncLoad() && JobSystem::Instance().GetMainThreadID() != std::this_thread::get_id())
    {
        requireMainThreadLoading = true;
        outLoader = std::move(loader);
        return nullptr;
    }

    return ExecuteLoader(loader, assetData, true);
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
        [this, projectRoot = this->projectRoot, &finishedJobs](std::pair<const UUID, AsyncProcessedPayload>& payload)
        {
            if (payload.second.requireMainThreadLoading)
            {
                auto& assetData = payload.second.assetData;
                auto& loader = payload.second.loader;

                if (loader == nullptr)
                    return;

                std::unique_ptr<Asset> newAsset = ExecuteLoader(loader, assetData, false);

                payload.second.loadedAsset = std::move(newAsset);
                payload.second.asset = payload.second.loadedAsset.get();
            }

            if (payload.second.loadedAsset)
            {
                payload.second.assetData->SetAsset(std::move(payload.second.loadedAsset), projectRoot);
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
