#include "AssetDatabase.hpp"
#include "Importers.hpp"
#include <iostream>
namespace Engine
{
AssetDatabase::AssetDatabase(const std::filesystem::path& projectRoot)
    : projectRoot(projectRoot), assetPath(projectRoot / "Assets"), assetDatabasePath(projectRoot / "AssetDatabase")
{
    if (!std::filesystem::exists(assetPath))
    {
        std::filesystem::create_directory(assetPath);
    }

    if (!std::filesystem::exists(assetDatabasePath))
    {
        std::filesystem::create_directory(assetDatabasePath);
    }

    // we need to load all the already imported asset when AssetDatabase starts so that when user load an asset we
    // know it's already in the database
    for (auto const& dirEntry : std::filesystem::directory_iterator{assetDatabasePath})
    {
        if (dirEntry.is_regular_file())
        {
            // assetData's file name is it's UUID
            UUID uuid(dirEntry.path().filename().string());
            auto ad = std::make_unique<AssetData>(dirEntry.path().filename().string(), projectRoot);

            if (ad->IsValid())
            {
                assets.Add(ad->GetAssetPath(), std::move(ad));
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
        SerializeAssetToDisk(asset, assetData->GetAssetPath());
    }
}

Asset* AssetDatabase::LoadAsset(std::filesystem::path path)
{
    // find the asset if it's already imported
    auto assetdata = assets.GetAssetData(path);
    path = assetPath / path;
    if (assetdata)
    {
        return assetdata->GetAsset();
    }

    if (!std::filesystem::exists(path))
        return nullptr;

    // see if the asset is an external asset(ktx, glb...), if so, start importing it
    std::filesystem::path ext = path.extension();
    auto asset = AssetRegistry::CreateAssetByExtension(ext.string());
    if (asset != nullptr)
    {
        if (asset->IsExternalAsset())
        {
            asset->LoadFromFile(path.string().c_str());
        }
        else
        {
            std::ifstream f(path, std::ios::binary);
            if (f.is_open() && f.good())
            {
                size_t fileSize = std::filesystem::file_size(path);
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
                    auto assetData = assets.GetAssetData(iter.first);
                    if (assetData)
                    {
                        ResolveAll(iter.second, assetData->GetAsset());
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

        Asset* temp = asset.get();
        std::unique_ptr<AssetData> ad = std::make_unique<AssetData>(std::move(asset), path, projectRoot);
        assets.Add(assetPath, std::move(ad));

        // see if there is any reference need to be resolved by this object
        auto iter = referenceResolveMap.find(temp->GetUUID());
        if (iter != referenceResolveMap.end())
        {
            for (auto& resolve : iter->second)
            {
                resolve.target = temp;
                if (resolve.callback)
                {
                    resolve.callback(temp);
                }
            }
            referenceResolveMap.erase(iter);
        }

        return temp;
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
    path = assetPath / path;
    // only internal asset can be created
    if (!a->IsExternalAsset() && !std::filesystem::exists(path))
    {
        path.replace_extension(a->GetExtension());
        std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
        Asset* asset = newAssetData->GetAsset();

        SerializeAssetToDisk(*asset, newAssetData->GetAssetPath());

        Asset* temp = assets.Add(path, std::move(newAssetData));

        return temp;
    }
    return nullptr;
}

Asset* AssetDatabase::Assets::Add(const std::filesystem::path& path, std::unique_ptr<AssetData>&& assetData)
{
    Asset* asset = assetData->GetAsset();
    if (asset)
    {
        byPath[path.string()] = assetData.get();
        data[asset->GetUUID()] = std::move(assetData);
    }
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

} // namespace Engine
