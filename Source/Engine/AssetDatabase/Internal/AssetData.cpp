#include "AssetData.hpp"

namespace Engine
{
AssetData::AssetData(
    std::unique_ptr<Asset>&& asset, const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot
)
    : assetDataUUID(), assetPath(assetPath), absolutePath(projectRoot / "Assets" / assetPath), asset(std::move(asset))
{
    for (auto obj : this->asset->GetInternalAssets())
    {
        nameToUUID[obj->GetName()] = obj->GetUUID().ToString();
    }
    isValid = false;
}

AssetData::AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot) : assetDataUUID(assetDataUUID)
{
    // load all the data until meta binary
    std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();

    if (!assetDataUUID.IsEmpty())
    {
        std::ifstream f(path);
        auto dataJson = nlohmann::json::parse(f);

        if (dataJson.empty())
        {
            isValid = false;
            return;
        }

        assetUUID = dataJson["assetUUID"];
        assetTypeID = dataJson["assetTypeID"];
        assetPath = dataJson.value("assetPath", "");
        absolutePath = projectRoot / std::filesystem::path("Assets") / assetPath;

        auto nameToUUIDJson = dataJson["nameToUUID"];
        if (nameToUUIDJson.is_object())
        {
            for (auto pair : nameToUUIDJson.items())
            {
                nameToUUID[pair.key()] = pair.value();
            }
        }

        isValid = true;
        return;
    }

    isValid = false;
    return;
}

AssetData::AssetData(const UUID& assetUUID, const std::filesystem::path& internalAssetPath, InternalAssetDataTag)
    : assetUUID(assetUUID), assetPath("_engine_intenral" / internalAssetPath),
      absolutePath(std::filesystem::absolute(std::filesystem::path("Assets") / internalAssetPath)), internal(true)
{
    isValid = true;
}

AssetData::~AssetData() {}

Asset* AssetData::GetAsset()
{
    if (asset != nullptr)
    {
        return asset.get();
    }

    return nullptr;
}

Asset* AssetData::SetAsset(std::unique_ptr<Asset>&& asset)
{
    asset->SetUUID(assetUUID);

    for (auto obj : asset->GetInternalAssets())
    {
        auto iter = nameToUUID.find(obj->GetName());
        if (iter != nameToUUID.end())
        {
            obj->SetUUID(iter->second);
        }
        else
        {
            nameToUUID[obj->GetName()] = obj->GetUUID().ToString();
            dirty = true;
        }
    }

    this->asset = std::move(asset);

    return this->asset.get();
}

void AssetData::SaveToDisk(const std::filesystem::path& projectRoot)
{
    if (!internal)
    {
        assetUUID = this->asset->GetUUID();
        assetTypeID = this->asset->GetObjectTypeID();

        std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();
        std::ofstream f(path);
        if (f.is_open() && f.good())
        {
            nlohmann::json j = {};
            j["assetUUID"] = assetUUID.ToString();
            j["assetTypeID"] = assetTypeID.ToString();
            j["assetPath"] = assetPath.string();

            for (auto& obj : nameToUUID)
            {
                j["nameToUUID"][obj.first] = obj.second.ToString();
            }

            f << j.dump();
            isValid = true;
            return;
        }

        dirty = false;
    }
}
} // namespace Engine
