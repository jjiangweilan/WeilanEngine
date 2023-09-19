#include "AssetData.hpp"

namespace Engine
{
AssetData::AssetData(
    std::unique_ptr<Asset>&& asset, const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot
)
    : asset(std::move(asset)), assetPath(assetPath), assetDataUUID()
{
    assetUUID = this->asset->GetUUID();
    assetTypeID = this->asset->GetAssetTypeID();

    std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();
    std::ofstream f(path);
    if (f.is_open() && f.good())
    {
        nlohmann::json j = {};
        j["assetUUID"] = assetUUID.ToString();
        j["assetTypeID"] = assetTypeID.ToString();
        j["assetPath"] = assetPath.string();
        f << j.dump();
        isValid = true;
        return;
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

        isValid = true;
        return;
    }

    isValid = false;
    return;
}

Asset* AssetData::GetAsset()
{
    if (asset != nullptr)
    {
        return asset.get();
    }

    asset = AssetRegistry::CreateAsset(assetTypeID);
    // asset->LoadFromFile(assetPath.string().c_str());
    return asset.get();
}
} // namespace Engine
