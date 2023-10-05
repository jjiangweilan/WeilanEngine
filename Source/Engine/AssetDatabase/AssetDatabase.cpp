#include "AssetDatabase.hpp"
#include "Importers.hpp"
#include <iostream>
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
}

void AssetDatabase::SaveAsset(Asset& asset)
{
    // this method only saves asset that is already imported
    AssetData* assetData = assets.GetAssetData(asset.GetUUID());

    if (assetData != nullptr)
    {
        SerializeAssetToDisk(asset, assetDirectory / assetData->GetAssetPath());
    }
}

Asset* AssetDatabase::LoadAsset(std::filesystem::path path)
{
    // find the asset if it's already imported
    auto assetData = assets.GetAssetData(path);
    auto fullAssetPath = assetDirectory / path;
    if (assetData)
    {
        auto a = assetData->GetAsset();
        if (a)
            return a;
    }

    if (!std::filesystem::exists(fullAssetPath))
        return nullptr;

    // see if the asset is an external asset(ktx, glb...), if so, start importing it
    std::filesystem::path ext = fullAssetPath.extension();
    auto newAsset = AssetRegistry::CreateAssetByExtension(ext.string());
    Asset* asset = newAsset.get();

    // if this is a asset in database we move the ownership into it's assetData
    if (assetData)
    {
        asset = assetData->SetAsset(std::move(newAsset));
    }
    if (asset != nullptr)
    {
        if (asset->IsExternalAsset())
        {
            asset->LoadFromFile(fullAssetPath.string().c_str());
        }
        else
        {
            std::ifstream f(fullAssetPath, std::ios::binary);
            if (f.is_open() && f.good())
            {
                size_t fileSize = std::filesystem::file_size(fullAssetPath);
                std::vector<uint8_t> binary(fileSize);
                f.read((char*)binary.data(), fileSize);
                SerializeReferenceResolveMap resolveMap;
                JsonSerializer ser(binary, &resolveMap);
                asset->Deserialize(&ser);

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

                // resolve reference
                for (auto& iter : resolveMap)
                {
                    // resolve external reference
                    auto externalAsset = LoadAssetByID(iter.first);
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

        // a new asset needs to be recored/imported in assetDatabase
        if (!assetData)
        {
            std::unique_ptr<AssetData> ad = std::make_unique<AssetData>(std::move(newAsset), path, projectRoot);
            assets.Add(std::move(ad));
        }

        // see if there is any reference need to be resolved by this object
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
        if (asset)
            return asset;

        return LoadAsset(assetData->GetAssetPath());
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
    auto fullPath = assetDirectory / path;
    // only internal asset can be created
    if (!a->IsExternalAsset() && !std::filesystem::exists(fullPath))
    {
        fullPath.replace_extension(a->GetExtension());
        std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
        Asset* asset = newAssetData->GetAsset();

        SerializeAssetToDisk(*asset, assetDirectory / newAssetData->GetAssetPath());

        Asset* temp = assets.Add(std::move(newAssetData));

        return temp;
    }
    return nullptr;
}

Asset* AssetDatabase::Assets::Add(std::unique_ptr<AssetData>&& assetData)
{
    Asset* asset = assetData->GetAsset();
    byPath[assetData->GetAssetPath().string()] = assetData.get();
    data[assetData->GetAssetUUID()] = std::move(assetData);
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
    auto iter = data.find(uuid);
    if (iter != data.end())
    {
        return iter->second.get();
    }
    return nullptr;
}

void AssetDatabase::SaveDirtyAssets()
{
    for (auto& a : assets.data)
    {
        Asset* asset = a.second->GetAsset();
        if (asset)
        {
            if (asset->IsDirty())
            {
                SaveAsset(*asset);
                asset->SetDirty(false);
            }
        }
    }
}

} // namespace Engine
