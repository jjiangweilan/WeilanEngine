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
    auto asset = AssetRegistry::CreateAssetByExtension(ext);
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

                // resolve reference
                for (auto& iter : resolveMap)
                {
                    auto assetData = assets.GetAssetData(iter.first);
                    if (assetData)
                    {
                        while (!iter.second.empty())
                        {
                            auto& toresolve = iter.second.back();
                            toresolve.target = assetData->GetAsset();
                            if (toresolve.callback)
                                toresolve.callback(assetData->GetAsset());

                            iter.second.pop_back();
                        }
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

Asset* AssetDatabase::SaveAsset(std::unique_ptr<Asset>&& a, std::filesystem::path path)
{
    path = assetPath / path;
    // only internal asset can be created
    if (!a->IsExternalAsset() && !std::filesystem::exists(path))
    {
        path.replace_extension(a->GetExtension());
        std::unique_ptr<AssetData> newAssetData = std::make_unique<AssetData>(std::move(a), path, projectRoot);
        Asset* asset = newAssetData->GetAsset();

        std::ofstream out;
        out.open(newAssetData->GetAssetPath(), std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
        if (out.is_open() && out.good())
        {
            JsonSerializer ser;
            asset->Serialize(&ser);
            auto binary = ser.GetBinary();

            if (binary.size() != 0)
            {
                out.write((char*)binary.data(), binary.size());
            }
        }
        out.close();

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
