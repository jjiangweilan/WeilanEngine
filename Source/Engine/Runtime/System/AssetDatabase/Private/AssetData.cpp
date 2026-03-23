#include "AssetData.hpp"
#include "Engine/Library/PodVector.hpp"
#include <spdlog/spdlog.h>

AssetData::AssetData(
    std::unique_ptr<Asset>&& asset, const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot
)
    : assetDataUUID(), assetPath(), absolutePath(), asset(std::move(asset)),
      lastWriteTime(0)
{
    SetAssetPath(assetPath, projectRoot / "Assets");
    assetUUID = this->asset->GetUUID();

    for (auto obj : this->asset->GetInternalAssets())
    {
        nameToUUID[GetNameToUUIDKey(obj)] = obj->GetUUID().ToString();
    }

    if (std::filesystem::exists(absolutePath))
    {
        lastWriteTime = std::filesystem::last_write_time(absolutePath).time_since_epoch().count();
    }
    isValid = false;
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

        assetUUID = std::string(dataJson["assetUUID"]);
        assetTypeID = std::string(dataJson["assetTypeID"]);
        std::string assetPathStr = dataJson.value("assetPath", "");
        SetAssetPath(assetPathStr, projectRoot / std::filesystem::path("Assets"));
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
                nameToUUID[pair.key()] = std::string(pair.value());
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

AssetData::AssetData() : assetUUID(), assetDataUUID(), lastWriteTime(0), assetTypeID(UUID::GetEmptyUUID()) {}

AssetData::~AssetData() {}

Asset* AssetData::GetAsset()
{
    if (asset != nullptr)
    {
        return asset.get();
    }

    return nullptr;
}

AssetData::AssetData(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot)
    : assetPath(), assetUUID(), absolutePath(), assetDataUUID(),
      lastWriteTime(0), assetTypeID(UUID::GetEmptyUUID())
{
    SetAssetPath(assetPath, projectRoot / "Assets");
}

Asset* AssetData::SetAsset(std::unique_ptr<Asset>&& inAsset, const std::filesystem::path& projectRoot)
{
    this->asset = std::move(inAsset); // note: asset UUID  will be updated laster in UpdateAssetUUIDs

    std::filesystem::path path = projectRoot / "AssetDatabase" / assetDataUUID.ToString();
    if (std::filesystem::exists(path))
    {
        std::filesystem::remove(path);
    }

    SaveToDisk(projectRoot);

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
            auto key = GetNameToUUIDKey(obj);
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
    }
    else
    {
        for (auto obj : asset->GetInternalAssets())
        {
            UUID concatUUID(fmt::format("{}-{}", assetUUID.ToString(), obj->GetName()), UUID::FromStrTag{});
            auto key = GetNameToUUIDKey(obj);
            obj->SetUUID(concatUUID);
            nameToUUID[key] = obj->GetUUID().ToString();
            dirty = true;
        }
    }
}

void AssetData::SaveToDisk(const std::filesystem::path& projectRoot)
{
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

nlohmann::json AssetData::DumpInfo() const
{
    nlohmann::json j = nlohmann::json::object();
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

std::filesystem::path AssetData::ToRelativeAssetPath(
    const std::filesystem::path& path, const std::filesystem::path& projectRoot
)
{
    if (path.empty())
        return path;

    auto genericStr = path.generic_string();

    // Already in the canonical relative format
    if (genericStr.starts_with("_engine_internal"))
        return path;

    // Already relative — assume caller has it right
    if (path.is_relative())
        return path;

    std::error_code ec;

    // Absolute path under projectRoot/Assets
    auto assetsDir = std::filesystem::weakly_canonical(projectRoot / "Assets", ec);
    if (!ec)
    {
        auto rel = std::filesystem::relative(path, assetsDir, ec);
        if (!ec && !rel.generic_string().starts_with(".."))
            return rel;
    }

    // Absolute path under cwd/Assets — engine-internal asset
    auto cwdAssetsDir = std::filesystem::weakly_canonical(std::filesystem::current_path() / "Assets", ec);
    if (!ec)
    {
        auto rel = std::filesystem::relative(path, cwdAssetsDir, ec);
        if (!ec && !rel.generic_string().starts_with(".."))
            return std::filesystem::path("_engine_internal") / rel;
    }

    return path;
}

std::string AssetData::GetNameToUUIDKey(Asset* obj)
{
    return fmt::format("{}-{}", obj->GetName(), ObjectRegistry::GetObjectTypeInfo(obj->GetObjectTypeID())->GetTypeName());
}
