#include "AssetDatabase.hpp"
#include "Importers.hpp"
#include <iostream>
#include <spdlog/spdlog.h>
namespace Engine
{
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

                auto ResolveAll = [](std::vector<SerializeReferenceResolve>& resolves, Object* resolved)
                {
                    while (!resolves.empty())
                    {
                        auto& toresolve = resolves.back();
                        toresolve.target = resolved;
                        if (toresolve.callback)
                            toresolve.callback(resolved);

                        resolves.pop_back();
                    }
                };

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

                // resolve reference
                for (auto& iter : resolveMap)
                {
                    // resolve external reference
                    Asset* externalAsset = LoadAssetByID(iter.first);
                    if (externalAsset)
                    {
                        ResolveAll(iter.second, externalAsset);
                    }

                    // resolve internal reference
                    // currently I didn't resolve reference to external contained object
                    // that can be done by cache a list of contained objects
                    auto objs = ser.GetContainedObjects();
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
    if (!a->IsExternalAsset() && !std::filesystem::exists(fullPath))
    {
        std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
        newAssetData->SaveToDisk(projectRoot);
        Asset* asset = newAssetData->GetAsset();

        SerializeAssetToDisk(*asset, newAssetData->GetAssetAbsolutePath());

        Asset* temp = assets.Add(std::move(newAssetData));

        return temp;
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
    auto iter = byPath.find(path.string());
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
    static auto f = [this](const UUID& id, const char* path)
    {
        auto assetData = std::make_unique<AssetData>(id, path, AssetData::InternalAssetDataTag{});
        internalAssets.push_back(assetData.get());
        if (assetData->IsValid())
        {
            auto temp = assetData.get();
            assets.Add(std::move(assetData));

            // we don't have a way to tell the contained asset inside an internal asset(unless we specify them manually)
            // we have to load them first so that other aseet can find the internal asset
            LoadAsset(temp->GetAssetPath());

            assets.UpdateAssetData(temp);
        }
    };

    f("118DF4BB-B41A-452A-BE48-CE95019AAF2E", "Shaders/Game/StandardPBR.shad");
    f("31D454BF-3D2D-46C4-8201-80377D12E1D2", "Shaders/Game/ShadowMap.shad");
    f("57F37367-05D5-4570-AFBB-C4146042B31E", "Shaders/Game/SimpleLit.shad");
    f("BABA4668-A5F3-40B2-92D3-1170C948DB63", "Models/Cube.glb");
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

void AssetDatabase::RequestShaderRefresh()
{
    requestShaderRefresh = true;
}

void AssetDatabase::RefreshShader()
{
    if (requestShaderRefresh)
    {
        GetGfxDriver()->WaitForIdle();

        requestShaderRefresh = false;
        for (auto& d : assets.data)
        {
            if (d->NeedRefresh())
            {
                auto asset = d->GetAsset();
                if (Shader* s = dynamic_cast<Shader*>(asset))
                {
                    try
                    {
                        s->LoadFromFile(d->GetAssetAbsolutePath().string().c_str());
                        d->UpdateAssetUUIDs();
                    }
                    catch (const std::exception& e)
                    {
                        SPDLOG_ERROR("failed to reload {}", d->GetAssetAbsolutePath().string());
                    }
                }
            }
        }
    }
}

} // namespace Engine
