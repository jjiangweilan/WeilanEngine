#include "AssetData.hpp"

AssetData::AssetData(
    std::unique_ptr<Asset>&& asset, const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot
)
    : assetDataUUID(), assetPath(assetPath), absolutePath(projectRoot / "Assets" / assetPath), asset(std::move(asset)),
      lastWriteTime(0)
{
    for (auto obj : this->asset->GetInternalAssets())
    {
        nameToUUID[obj->GetName()] = obj->GetUUID().ToString();
    }

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
    isValid = false;
}

AssetData::AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot)
    : assetDataUUID(assetDataUUID), lastWriteTime(0)
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
        std::string assetPathStr = dataJson.value("assetPath", "");
        assetPath = assetPathStr;
        meta = dataJson.value("meta", nlohmann::json::object());

        this->internal = true;
        for (int i = 0; i < 16; ++i)
        {
            if (assetPathStr[i] != "_engine_internal"[i])
            {
                internal = false;
                break;
            }
        }
        if (internal)
        {
            std::replace(assetPathStr.begin(), assetPathStr.end(), '\\', '/');
            auto realPath = assetPathStr.substr(17);
            auto currentPath = std::filesystem::current_path();
            absolutePath = currentPath / std::filesystem::path("Assets") / realPath;
        }
        else
        {
            absolutePath = projectRoot / std::filesystem::path("Assets") / assetPath;
        }
        auto nameToUUIDJson = dataJson["nameToUUID"];
        if (nameToUUIDJson.is_object())
        {
            for (auto pair : nameToUUIDJson.items())
            {
                nameToUUID[pair.key()] = pair.value();
            }
        }

        isValid = true;

        if (std::filesystem::exists(absolutePath))
        {
            lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
        }

        return;
    }

    isValid = false;
    return;
}

AssetData::AssetData(const UUID& assetUUID, const std::filesystem::path& internalAssetPath, InternalAssetDataTag)
    : assetUUID(assetUUID), lastWriteTime(0), assetPath("_engine_internal" / internalAssetPath),
      absolutePath(std::filesystem::absolute(std::filesystem::path("Assets") / internalAssetPath)), internal(true)
{
    isValid = true;

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
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
    if (this->asset != nullptr)
        this->asset->Reload(std::move(*asset));
    else
        this->asset = std::move(asset);

    UpdateAssetUUIDs();

    return this->asset.get();
}

void AssetData::UpdateAssetUUIDs()
{
    asset->SetUUID(assetUUID);

    if (!internal)
    {
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
    }
    else
    {
        for (auto obj : asset->GetInternalAssets())
        {
            UUID concatUUID(fmt::format("{}-{}", assetUUID.ToString(), obj->GetName()), UUID::FromStrTag{});
            obj->SetUUID(concatUUID);
            nameToUUID[obj->GetName()] = obj->GetUUID().ToString();
            dirty = true;
        }
    }
}

void AssetData::SaveToDisk(const std::filesystem::path& projectRoot)
{
    assetUUID = this->asset->GetUUID();
    assetTypeID = this->asset->GetObjectTypeID();

    std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();
    std::ofstream f(path, std::ios::trunc);
    if (f.is_open() && f.good())
    {
        nlohmann::json j = DumpInfo();

        f << j.dump();
        isValid = true;
        return;
    }

    dirty = false;
}

bool AssetData::NeedRefresh() const
{
    if (std::filesystem::exists(absolutePath))
    {
        auto newWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
        if (newWriteTime > lastWriteTime || (asset && asset->NeedReimport()))
            return true;
    }

    return false;
}

void AssetData::UpdateLastWriteTime()
{
    lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
}

bool AssetData::ChangeAssetPath(const std::filesystem::path& path, const std::filesystem::path& projectRoot)
{
    auto newAbsolutePath = projectRoot / "Assets" / path;

    try
    {
        std::filesystem::rename(absolutePath, newAbsolutePath);
    }
    catch (...)
    {
        return false;
    }

    assetPath = path;
    absolutePath = newAbsolutePath;
    UpdateLastWriteTime();
    SaveToDisk(projectRoot);
}

nlohmann::json AssetData::DumpInfo() const
{
    nlohmann::json j = {};
    j["assetUUID"] = assetUUID.ToString();
    j["assetTypeID"] = assetTypeID.ToString();
    auto assetPathStr = assetPath.string();
    std::replace(assetPathStr.begin(), assetPathStr.end(), '\\', '/');
    j["assetPath"] = assetPathStr;
    j["meta"] = meta;

    for (auto& obj : nameToUUID)
    {
        j["nameToUUID"][obj.first] = obj.second.ToString();
    }

    return j;
}
