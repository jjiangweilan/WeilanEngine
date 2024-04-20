#include "AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#include "Importers.hpp"
#include "Libs/Profiler.hpp"
#include <future>
#include <iostream>
#include <spdlog/spdlog.h>

AssetDatabase::AssetDatabase(const std::filesystem::path& projectRoot)
    : projectRoot(projectRoot), assetDirectory(projectRoot / "Assets"),
      assetDatabaseDirectory(projectRoot / "AssetDatabase")
{
    if (!std::filesystem::exists(assetDirectory))
    {
        std::filesystem::create_directory(assetDirectory);
    }

    if (!std::filesystem::exists(assetDatabaseDirectory))
    {
        std::filesystem::create_directory(assetDatabaseDirectory);
    }

    // we need to load all the already imported asset when AssetDatabase starts so that when user load an asset we
    // know it's already in the database
    for (auto const& dirEntry : std::filesystem::directory_iterator{assetDatabaseDirectory})
    {
        if (dirEntry.is_regular_file())
        {
            // assetData's file name is it's UUID
            UUID uuid(dirEntry.path().filename().string());
            auto ad = std::make_unique<AssetData>(dirEntry.path().filename().string(), projectRoot);

            if (ad->IsValid())
            {
                assets.Add(std::move(ad));
            }
            else
            {
                // TODO: is AssetData is not valid... do something with it!
            }
        }
    }

    LoadEngineInternal();
}

void AssetDatabase::SaveAsset(Asset& asset)
{
    // this method only saves asset that is already imported
    AssetData* assetData = assets.GetAssetData(asset.GetUUID());

    if (assetData != nullptr)
    {
        SerializeAssetToDisk(asset, assetData->GetAssetAbsolutePath());
    }
}

bool AssetDatabase::IsAssetInDatabase(Asset& asset)
{
    return assets.GetAssetData(asset.GetUUID()) != nullptr;
}

std::vector<Asset*> AssetDatabase::LoadAssets(std::span<std::filesystem::path> pathes)
{
    struct AsyncImport
    {
        // 0: init, 1: imported, 2: invalid, 3. async external import 4. async internal import
        int stateTrack;
        std::filesystem::path absoluteAssetPath;
        AssetData* assetData = nullptr;
        std::unique_ptr<Asset> newAsset = nullptr;
        std::future<void> import;
        std::unique_ptr<SerializeReferenceResolveMap> resolveMap = nullptr;
        std::unique_ptr<JsonSerializer> ser = nullptr;
    };
    const int size = pathes.size();
    std::vector<Asset*> results(size, nullptr);
    std::vector<AsyncImport> asyncImport(size);

    for (int i = 0; i < pathes.size(); ++i)
    {
        auto& path = pathes[i];
        auto assetData = assets.GetAssetData(path);
        asyncImport[i].assetData = assetData;
        asyncImport[i].absoluteAssetPath = assetDirectory / path;
        if (assetData)
        {
            // override the asset path because this asset may be an internal asset
            asyncImport[i].absoluteAssetPath = assetData->GetAssetAbsolutePath();
            auto a = assetData->GetAsset();
            if (a)
            {
                results[i] = a;
                asyncImport[i].stateTrack = 1;
            }
        }

        if (!std::filesystem::exists(asyncImport[i].absoluteAssetPath))
        {
            asyncImport[i].stateTrack = 2;
        }
    }

    for (int i = 0; i < size; ++i)
    {
        if (asyncImport[i].stateTrack == 0)
        {
            std::filesystem::path ext = asyncImport[i].absoluteAssetPath.extension();
            asyncImport[i].newAsset = AssetRegistry::CreateAssetByExtension(ext.string());

            if (asyncImport[i].newAsset->IsExternalAsset())
            {
                asyncImport[i].stateTrack = 3;
                asyncImport[i].import = std::async(
                    std::launch::async,
                    [asyncImport = &asyncImport[i]]()
                    { asyncImport->newAsset->LoadFromFile(asyncImport->absoluteAssetPath.string().c_str()); }
                );
            }
            else
            {
                std::ifstream f(asyncImport[i].absoluteAssetPath, std::ios::binary);
                if (f.is_open() && f.good())
                {
                    asyncImport[i].stateTrack = 4;
                    size_t fileSize = std::filesystem::file_size(asyncImport[i].absoluteAssetPath);
                    std::vector<uint8_t> binary(fileSize);
                    f.read((char*)binary.data(), fileSize);
                    asyncImport[i].resolveMap = std::make_unique<SerializeReferenceResolveMap>();
                    asyncImport[i].ser = std::make_unique<JsonSerializer>(binary, asyncImport[i].resolveMap.get());
                    asyncImport[i].import = std::async(
                        std::launch::async,
                        [asyncImport = &asyncImport[i]]()
                        { asyncImport->newAsset->Deserialize(asyncImport->ser.get()); }
                    );
                }
                else
                {
                    asyncImport[i].stateTrack = 1;
                }
            }
        }
    }

    for (int i = 0; i < size; ++i)
    {
        if (asyncImport[i].stateTrack == 3 || asyncImport[i].stateTrack == 4)
        {
            asyncImport[i].import.wait();
            results[i] = asyncImport[i].newAsset.get();
            if (asyncImport[i].assetData)
            {
                asyncImport[i].assetData->SetAsset(std::move(asyncImport[i].newAsset));
            }
            // a new asset needs to be recored/imported in assetDatabase
            else
            {
                std::unique_ptr<AssetData> ad =
                    std::make_unique<AssetData>(std::move(asyncImport[i].newAsset), pathes[i], projectRoot);
                ad->SaveToDisk(projectRoot);
                assets.Add(std::move(ad));
            }
        }

        if (asyncImport[i].stateTrack == 4)
        {
            const auto& managedObjectCounters = asyncImport[i].ser->GetManagedObjects();
            this->managedObjectCounters.insert(managedObjectCounters.begin(), managedObjectCounters.end());

            auto ResolveAll = [this](std::vector<SerializeReferenceResolve>& resolves, Object* resolved)
            {
                while (!resolves.empty())
                {
                    auto& toresolve = resolves.back();
                    toresolve.target = resolved;
                    if (toresolve.callback)
                        toresolve.callback(resolved);

                    if (toresolve.managedObjectRefCounter)
                    {
                        auto counterIter = this->managedObjectCounters.find(resolved->GetUUID());
                        if (counterIter != this->managedObjectCounters.end())
                        {
                            *toresolve.managedObjectRefCounter = counterIter->second;
                            if (*toresolve.managedObjectRefCounter)
                            {
                                **toresolve.managedObjectRefCounter += 1;
                            }
                        }
                    }

                    resolves.pop_back();
                }
            };
            for (auto& iter : *asyncImport[i].resolveMap)
            {
                if (iter.second.empty())
                    continue;
                // resolve external reference
                Asset* externalAsset = LoadAssetByID(iter.first);
                if (externalAsset)
                {
                    ResolveAll(iter.second, externalAsset);
                }

                // resolve internal reference
                // currently I didn't resolve reference to external contained object
                // that can be done by cache a list of contained objects
                const auto& objs = (*asyncImport[i].ser).GetContainedObjects();
                auto containedObj = objs.find(iter.first);
                if (containedObj != objs.end())
                {
                    auto resolved = containedObj->second;
                    ResolveAll(iter.second, resolved);
                }

                // add whatever is not resolved to assetDatabase's resolve map
                if (!iter.second.empty())
                {
                    auto& vec = referenceResolveMap[iter.first];
                    for (auto& r : iter.second)
                    {
                        vec.emplace_back(r.target, r.targetUUID, r.callback);
                    }
                }
            }
        }

        // see if there is any reference need to be resolved to this object
        auto iter = referenceResolveMap.find(results[i]->GetUUID());
        if (iter != referenceResolveMap.end())
        {
            for (auto& resolve : iter->second)
            {
                resolve.target = results[i];
                if (resolve.callback)
                {
                    resolve.callback(results[i]);
                }
            }
            referenceResolveMap.erase(iter);
        }
    }

    return results;
}

Asset* AssetDatabase::LoadAsset(std::filesystem::path path)
{
    // find the asset if it's already imported
    auto assetData = assets.GetAssetData(path);
    auto absoluteAssetPath = assetDirectory / path;
    if (assetData)
    {
        // override the asset path because this asset may be an internal asset
        absoluteAssetPath = assetData->GetAssetAbsolutePath();
        auto a = assetData->GetAsset();
        if (a)
            return a;
    }

    if (!std::filesystem::exists(absoluteAssetPath))
        return nullptr;

    // this asset is going to be loaded from disk, start the profiler
    // SCOPED_PROFILER(path.string());

    // see if the asset is an external asset(ktx, glb...), if so, start importing it
    std::filesystem::path ext = absoluteAssetPath.extension();
    auto newAsset = AssetRegistry::CreateAssetByExtension(ext.string());

    if (newAsset != nullptr)
    {
        Asset* asset = newAsset.get();

        if (newAsset->IsExternalAsset())
        {
            newAsset->LoadFromFile(absoluteAssetPath.string().c_str());
            if (assetData)
            {
                assetData->SetAsset(std::move(newAsset));
            }
            else
            {
                std::unique_ptr<AssetData> ad = std::make_unique<AssetData>(std::move(newAsset), path, projectRoot);
                ad->SaveToDisk(projectRoot);
                assets.Add(std::move(ad));
            }
        }
        else
        {
            std::ifstream f(absoluteAssetPath, std::ios::binary);
            if (f.is_open() && f.good())
            {
                size_t fileSize = std::filesystem::file_size(absoluteAssetPath);
                std::vector<uint8_t> binary(fileSize);
                f.read((char*)binary.data(), fileSize);
                SerializeReferenceResolveMap resolveMap;
                JsonSerializer ser(binary, &resolveMap);
                newAsset->Deserialize(&ser);

                if (assetData)
                {
                    // this needs to be done after importing becuase if not we don't have internal game object's name to
                    // set UUID by SetAsset(implementation detail leakage, refactor may be needed). It also needs to
                    // happen before reference resolve so that it has the correct UUID
                    assetData->SetAsset(std::move(newAsset));
                }
                // a new asset needs to be recored/imported in assetDatabase
                else
                {
                    std::unique_ptr<AssetData> ad = std::make_unique<AssetData>(std::move(newAsset), path, projectRoot);
                    ad->SaveToDisk(projectRoot);
                    assets.Add(std::move(ad));
                }

                const auto& managedObjectCounters = ser.GetManagedObjects();
                this->managedObjectCounters.insert(managedObjectCounters.begin(), managedObjectCounters.end());

                auto ResolveAll = [this](std::vector<SerializeReferenceResolve>& resolves, Object* resolved)
                {
                    while (!resolves.empty())
                    {
                        auto& toresolve = resolves.back();
                        toresolve.target = resolved;
                        if (toresolve.callback)
                            toresolve.callback(resolved);

                        if (toresolve.managedObjectRefCounter)
                        {
                            auto counterIter = this->managedObjectCounters.find(resolved->GetUUID());
                            if (counterIter != this->managedObjectCounters.end())
                            {
                                *toresolve.managedObjectRefCounter = counterIter->second;
                                if (*toresolve.managedObjectRefCounter)
                                {
                                    **toresolve.managedObjectRefCounter += 1;
                                }
                            }
                        }

                        resolves.pop_back();
                    }
                };
                for (auto& iter : resolveMap)
                {
                    if (iter.second.empty())
                        continue;
                    // resolve external reference
                    Asset* externalAsset = LoadAssetByID(iter.first);
                    if (externalAsset)
                    {
                        ResolveAll(iter.second, externalAsset);
                    }

                    // resolve internal reference
                    // currently I didn't resolve reference to external contained object
                    // that can be done by cache a list of contained objects
                    const auto& objs = ser.GetContainedObjects();
                    auto containedObj = objs.find(iter.first);
                    if (containedObj != objs.end())
                    {
                        auto resolved = containedObj->second;
                        ResolveAll(iter.second, resolved);
                    }

                    // add whatever is not resolved to assetDatabase's resolve map
                    if (!iter.second.empty())
                    {
                        auto& vec = referenceResolveMap[iter.first];
                        for (auto& r : iter.second)
                        {
                            vec.emplace_back(r.target, r.targetUUID, r.callback, r.managedObjectRefCounter);
                        }
                    }
                }
            }
        }

        // see if there is any reference need to be resolved to this object
        auto iter = referenceResolveMap.find(asset->GetUUID());
        if (iter != referenceResolveMap.end())
        {
            for (auto& resolve : iter->second)
            {
                resolve.target = asset;
                if (resolve.callback)
                {
                    resolve.callback(asset);
                }
            }
            referenceResolveMap.erase(iter);
        }

        return asset;
    }

    return nullptr;
}

Asset* AssetDatabase::LoadAssetByID(const UUID& uuid)
{
    auto assetData = assets.GetAssetData(uuid);
    if (assetData)
    {
        auto asset = assetData->GetAsset();
        if (!asset)
        {
            asset = LoadAsset(assetData->GetAssetPath());
        }

        if (asset)
        {
            if (asset->GetUUID() == uuid)
            {
                return asset;
            }
            else
            {
                auto internalAssets = asset->GetInternalAssets();
                auto iter = std::find_if(
                    internalAssets.begin(),
                    internalAssets.end(),
                    [&uuid](Asset* a) { return a->GetUUID() == uuid; }
                );
                if (iter != internalAssets.end())
                    return *iter;
            }
        }
    }

    return nullptr;
}

void AssetDatabase::SerializeAssetToDisk(Asset& asset, const std::filesystem::path& path)
{
    JsonSerializer ser;
    asset.Serialize(&ser);
    auto binary = ser.GetBinary();
    if (binary.size() != 0)
    {
        std::ofstream out;
        out.open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        if (out.is_open() && out.good())
        {
            out.write((char*)binary.data(), binary.size());
        }
    }
}
Asset* AssetDatabase::SaveAsset(std::unique_ptr<Asset>&& a, std::filesystem::path path)
{
    if (path.is_absolute())
        return nullptr;

    path.replace_extension(a->GetExtension());
    auto fullPath = assetDirectory / path;
    // only internal asset can be created
    if (!a->IsExternalAsset())
    {
        if (!std::filesystem::exists(fullPath))
        {
            std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
            newAssetData->SaveToDisk(projectRoot);
            Asset* asset = newAssetData->GetAsset();

            SerializeAssetToDisk(*asset, newAssetData->GetAssetAbsolutePath());

            Asset* temp = assets.Add(std::move(newAssetData));

            return temp;
        }
        else
        {
            if (AssetData* ad = assets.GetAssetData(path))
            {
                if (Asset* asset = ad->GetAsset())
                {
                    asset->Reload(std::move(*a));
                    SerializeAssetToDisk(*asset, ad->GetAssetAbsolutePath());
                }
            }
        }
    }
    return nullptr;
}

Asset* AssetDatabase::Assets::Add(std::unique_ptr<AssetData>&& assetData)
{
    Asset* asset = assetData->GetAsset();
    UpdateAssetData(assetData.get());
    data.push_back(std::move(assetData));

    return asset;
}

AssetData* AssetDatabase::Assets::GetAssetData(const std::filesystem::path& path)
{
    auto iter = byPath.find(path);
    if (iter != byPath.end())
    {
        return iter->second;
    }

    return nullptr;
}
AssetData* AssetDatabase::Assets::GetAssetData(const UUID& uuid)
{
    auto iter = byUUID.find(uuid);
    if (iter != byUUID.end())
    {
        return iter->second;
    }
    return nullptr;
}

void AssetDatabase::SaveDirtyAssets()
{
    for (auto& a : assets.data)
    {
        Asset* asset = a->GetAsset();
        if (asset)
        {
            if (asset->IsDirty())
            {
                SaveAsset(*asset);
                asset->SetDirty(false);
            }
        }

        if (a->IsDirty())
        {
            a->SaveToDisk(projectRoot);
        }
    }
}

void AssetDatabase::LoadEngineInternal()
{
    std::vector<std::string> pathes;
    for (auto entry : std::filesystem::recursive_directory_iterator("./Assets"))
    {
        if (!entry.is_directory())
        {
            auto relative = std::filesystem::relative(entry.path(), "./Assets/");
            if (AssetRegistry::IsExtensionAnAsset(relative.extension().string()))
            {
                auto str = relative.string();
                std::replace(str.begin(), str.end(), '\\', '/');
                pathes.push_back(str);
            }
        }
    }

    std::vector<std::filesystem::path> importPathes;
    std::vector<AssetData*> validAssetData;
    for (int i = 0; i < pathes.size(); ++i)
    {
        auto assetData = std::make_unique<AssetData>(
            UUID(pathes[i], UUID::FromStrTag{}),
            pathes[i],
            AssetData::InternalAssetDataTag{}
        );
        if (assetData->IsValid())
        {
            AssetData* tmp = assetData.get();
            internalAssets.push_back(tmp);
            validAssetData.push_back(tmp);
            assets.Add(std::move(assetData));
            importPathes.push_back(tmp->GetAssetPath());
        }
    }

    auto shaderIter = std::partition(
        importPathes.begin(),
        importPathes.end(),
        [](std::filesystem::path& path) { return path.extension() == ".shad"; }
    );
    std::vector<std::filesystem::path> shaderPathes(importPathes.begin(), shaderIter);
    LoadAssets(shaderPathes);
    Shader* standardShader = (Shader*)LoadAsset("_engine_internal/Shaders/Game/StandardPBR.shad");
    Shader::SetDefault(standardShader);

    std::vector<std::filesystem::path> others(shaderIter, importPathes.end());
    LoadAssets(others);

    for (auto a : validAssetData)
    {
        assets.UpdateAssetData(a);
    }
}

void AssetDatabase::Assets::UpdateAssetData(AssetData* assetData)
{
    byPath[assetData->GetAssetPath().string()] = assetData;
    byUUID[assetData->GetAssetUUID()] = assetData;

    for (auto& iter : assetData->GetInternalObjectAssetNameToUUID())
    {
        byUUID[iter.second] = assetData;
    }
}

void AssetDatabase::RequestShaderRefresh(bool all)
{
    requestShaderRefresh = true;
    requestShaderRefreshAll = all;
}

void AssetDatabase::RefreshShader()
{
    if (requestShaderRefresh)
    {
        GetGfxDriver()->WaitForIdle();

        requestShaderRefresh = false;
        for (auto& d : assets.data)
        {
            if (requestShaderRefreshAll || d->NeedRefresh())
            {
                auto asset = d->GetAsset();
                if (ShaderBase* s = dynamic_cast<ShaderBase*>(asset))
                {
                    try
                    {
                        if (s->LoadFromFile(d->GetAssetAbsolutePath().string().c_str()))
                        {
                            SPDLOG_INFO("Shader reloaded: {}", d->GetAssetPath().string());
                        }
                        d->UpdateAssetUUIDs();
                        d->UpdateLastWriteTime();
                    }
                    catch (const std::exception& e)
                    {
                        SPDLOG_ERROR("failed to reload {}", d->GetAssetAbsolutePath().string());
                    }
                }
            }
        }

        requestShaderRefreshAll = false;
    }
}

bool AssetDatabase::ChangeAssetPath(const std::filesystem::path& src, const std::filesystem::path& dst)
{
    AssetData* data = assets.GetAssetData(src);
    if (data)
    {
        if (data->ChangeAssetPath(dst, projectRoot))
        {
            assets.byPath.erase(src);
            assets.byPath[dst] = data;
            return true;
        }
        return false;
    }
    return false;
}

AssetDatabase*& AssetDatabase::SingletonReference()
{
    static AssetDatabase* instance;
    return instance;
}
