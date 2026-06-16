#include "AssetData.hpp"
#include "Engine/Library/PodVector.hpp"
#include <spdlog/spdlog.h>
#include <unordered_set>

namespace
{
bool TryLoadJsonFile(const std::filesystem::path& path, nlohmann::json& out)
{
    if (!std::filesystem::exists(path))
    {
        return false;
    }

    try
    {
        std::ifstream file(path);
        if (file.good() && file.peek() != std::ifstream::traits_type::eof())
        {
            out = nlohmann::json::parse(file);
            return true;
        }
    }
    catch (...)
    {
        spdlog::warn("failed to load json at {}", path.string());
    }

    return false;
}
} // namespace

AssetData::AssetData(
    std::unique_ptr<Asset>&& asset, const AssetPath& assetPath, const std::filesystem::path& projectRoot
)
    : assetDataUUID(), assetPath(), absolutePath(), asset(std::move(asset)),
      lastWriteTime(0)
{
    (void)projectRoot;
    SetAssetPath(assetPath);
    assetUUID = this->asset->GetUUID();
    assetTypeID = this->asset->GetObjectTypeID();

    for (auto obj : this->asset->GetInternalAssets())
    {
        nameToUUID[GetNameToUUIDKey(obj)] = obj->GetUUID().ToString();
    }

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
    dirty = true;
    isValid = true;
}

static PodVector<uint8_t> ReadFile(const std::filesystem::path& path)
{
    std::ifstream f;
    // f.rdbuf()->pubsetbuf(streamBuf.data(), streamBufSize);
    f.open(path, std::ios::binary);
    if (!f.good())
        return {};
    auto fileSize = std::filesystem::file_size(path);
    PodVector<uint8_t> d(fileSize);
    f.read((char*)d.data(), fileSize);
    return d;
}

AssetData::AssetData(const UUID& assetDataUUID, const std::filesystem::path& projectRoot)
    : assetDataUUID(assetDataUUID), lastWriteTime(0)
{

    // load all the data until meta binary
    std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();

    if (!assetDataUUID.IsEmpty())
    {
        if (!std::filesystem::exists(path))
        {
            isValid = false;
            return;
        }

        nlohmann::json dataJson;

        try
        {
            std::ifstream file(path);
            if (file.good() && file.peek() != std::ifstream::traits_type::eof())
            {
                dataJson = nlohmann::json::parse(file); // seems nlohmann::json::parse() having issue with VS 2026 build, we need to make sure the json is valid
            }
        }
        catch (...)
        {
            spdlog::warn("failed to load AssetData at {}", path.string());
            isValid = false;
            return;
        }
        if (dataJson.empty())
        {
            isValid = false;
            return;
        }

        AssetPath loadedAssetPath = dataJson.value("assetPath", "");
        SetAssetPath(loadedAssetPath);
        LoadSharedFieldsFromJson(dataJson);

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

AssetData::AssetData(const UUID& assetUUID, const AssetPath& internalAssetPath, InternalAssetDataTag)
    : assetUUID(assetUUID), lastWriteTime(0), assetPath(internalAssetPath),
      absolutePath(internalAssetPath.ToAbsolutePath()), internal(true)
{
    nlohmann::json metaJson;
    if (TryLoadJsonFile(GetMetaAbsolutePath(), metaJson))
    {
        LoadSharedFieldsFromJson(metaJson);
    }

    if (this->assetUUID.IsEmpty())
    {
        this->assetUUID = assetUUID;
    }

    isValid = true;

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
}

AssetData::AssetData() : assetUUID(), assetDataUUID(), lastWriteTime(0), assetTypeID(UUID::GetEmptyUUID()) {}

AssetData::~AssetData() {}

void AssetData::RegenerateAssetUUID()
{
    assetUUID = UUID();
    dirty = true;
}

void AssetData::ClearInternalObjectUUIDs()
{
    nameToUUID.clear();
    dirty = true;
}

Asset* AssetData::GetAsset()
{
    if (asset != nullptr)
    {
        return asset.get();
    }

    return nullptr;
}

AssetData::AssetData(const AssetPath& assetPath, const std::filesystem::path& projectRoot)
    : assetPath(), assetUUID(), absolutePath(), assetDataUUID(),
      lastWriteTime(0), assetTypeID(UUID::GetEmptyUUID())
{
    (void)projectRoot;
    SetAssetPath(assetPath);

    nlohmann::json metaJson;
    if (TryLoadJsonFile(GetMetaAbsolutePath(), metaJson))
    {
        LoadSharedFieldsFromJson(metaJson);
    }
    else
    {
        assetUUID = internal ? UUID(assetPath.string(), UUID::FromStrTag{}) : UUID();
        dirty = !internal;
    }

    isValid = true;

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
}

Asset* AssetData::SetAsset(std::unique_ptr<Asset>&& inAsset, const std::filesystem::path& projectRoot)
{
    this->asset = std::move(inAsset); // note: asset UUID  will be updated laster in UpdateAssetUUIDs
    assetTypeID = this->asset->GetObjectTypeID();

    UpdateAssetUUIDs();
    SaveToDisk(projectRoot);
    return this->asset.get();
}

void AssetData::UpdateAssetUUIDs()
{
    asset->SetUUID(assetUUID);

    if (!internal)
    {
        std::unordered_set<std::string> currentKeys;
        for (auto obj : asset->GetInternalAssets())
        {
            auto key = GetNameToUUIDKey(obj);
            currentKeys.insert(key);
            auto iter = nameToUUID.find(key);
            if (iter != nameToUUID.end())
            {
                obj->SetUUID(iter->second);
            }
            else
            {
                nameToUUID[key] = obj->GetUUID().ToString();
                dirty = true;
            }
        }

        for (auto iter = nameToUUID.begin(); iter != nameToUUID.end();)
        {
            if (!currentKeys.contains(iter->first))
            {
                iter = nameToUUID.erase(iter);
                dirty = true;
            }
            else
            {
                ++iter;
            }
        }
    }
    else
    {
        for (auto obj : asset->GetInternalAssets())
        {
            UUID concatUUID(fmt::format("{}-{}", assetUUID.ToString(), obj->GetName()), UUID::FromStrTag{});
            auto key = GetNameToUUIDKey(obj);
            obj->SetUUID(concatUUID);
            nameToUUID[key] = obj->GetUUID().ToString();
        }
    }
}

void AssetData::SaveToDisk(const std::filesystem::path& projectRoot)
{
    (void)projectRoot;
    std::filesystem::path path = GetMetaAbsolutePath();

    if (internal && !dirty && !std::filesystem::exists(path))
    {
        isValid = true;
        return;
    }

    std::ofstream f(path, std::ios::trunc);
    if (f.is_open() && f.good())
    {
        nlohmann::json j = DumpInfo();

        f << j.dump();
        isValid = true;
        dirty = false;
        return;
    }

    spdlog::warn("failed to save AssetData meta at {}", path.string());
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

nlohmann::json AssetData::DumpInfo() const
{
    nlohmann::json j = nlohmann::json::object();
    j["version"] = 1;
    j["assetUUID"] = assetUUID.ToString();
    j["assetTypeID"] = assetTypeID.ToString();
    j["assetPath"] = assetPath.string();
    j["meta"] = meta;

    for (auto& obj : nameToUUID)
    {
        j["nameToUUID"][obj.first] = obj.second.ToString();
    }

    return j;
}

std::string AssetData::GetNameToUUIDKey(Asset* obj)
{
    return fmt::format("{}-{}", obj->GetName(), ObjectRegistry::GetObjectTypeInfo(obj->GetObjectTypeID())->GetTypeName());
}

void AssetData::LoadSharedFieldsFromJson(const nlohmann::json& dataJson)
{
    auto assetUUIDStr = dataJson.value("assetUUID", "");
    assetUUID = assetUUIDStr.empty() ? UUID() : UUID(assetUUIDStr);
    assetTypeID = dataJson.value("assetTypeID", UUID::GetEmptyUUID().ToString());
    meta = dataJson.value("meta", nlohmann::json::object());
    nameToUUID.clear();

    auto nameToUUIDJson = dataJson.value("nameToUUID", nlohmann::json::object());
    if (nameToUUIDJson.is_object())
    {
        for (auto pair : nameToUUIDJson.items())
        {
            nameToUUID[pair.key()] = std::string(pair.value());
        }
    }
}

std::filesystem::path AssetData::GetMetaAbsolutePath() const
{
    return std::filesystem::path(absolutePath.string() + ".meta");
}
