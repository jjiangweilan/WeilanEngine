#include "AssetFileSystem.hpp"

namespace
{
bool IsMetaFile(const std::filesystem::path& path)
{
    return path.extension() == ".meta";
}

std::filesystem::path GetMetaPath(const std::filesystem::path& assetPath)
{
    return std::filesystem::path(assetPath.string() + ".meta");
}
} // namespace

void AssetFileSystem::Init(const std::filesystem::path& projectRoot)
{
    this->projectRoot = projectRoot;
    this->assetDirectory = projectRoot / "Assets";
}

Asset* AssetFileSystem::Add(AssetData* assetData)
{
    Asset* asset = assetData->GetAsset();
    UpdateAssetData(assetData);

    return asset;
}

AssetData* AssetFileSystem::GetAssetData(const AssetPath& path) const
{
    auto iter = byPath.find(path);
    if (iter != byPath.end())
    {
        return iter->second;
    }

    return nullptr;
}

AssetData* AssetFileSystem::GetAssetData(const UUID& uuid) const
{
    auto iter = byUUID.find(uuid);
    if (iter != byUUID.end())
    {
        return iter->second;
    }
    return nullptr;
}

void AssetFileSystem::UpdateAssetData(AssetData* assetData)
{
    byPath[assetData->GetAssetPath()] = assetData;
    byUUID[assetData->GetAssetUUID()] = assetData;

    for (auto& iter : assetData->GetInternalObjectAssetNameToUUID())
    {
        byUUID[iter.second] = assetData;
    }
}

void AssetFileSystem::Rename(const AssetPath& oldPath, const AssetPath& newPath)
{
    if (oldPath == newPath)
        return;

    auto fullNewPath = GetAssetDirectory() / newPath.ToFilesystemPath();
    auto fullOldPath = GetAssetDirectory() / oldPath.ToFilesystemPath();
    if (!std::filesystem::exists(fullNewPath.parent_path()) || !std::filesystem::exists(fullOldPath))
    {
        return;
    }

    // collect all data before we actually move any file
    std::vector<std::pair<AssetData*, AssetPath>> movedAssets;

    if (std::filesystem::is_directory(fullOldPath))
    {
        for (auto entry : std::filesystem::recursive_directory_iterator(fullOldPath))
        {
            if (entry.is_regular_file() && !IsMetaFile(entry.path()))
            {
                auto relativeAssetPath = AssetPath(std::filesystem::relative(entry.path(), GetAssetDirectory()));
                AssetData* assetData = GetAssetData(relativeAssetPath);
                if (assetData != nullptr)
                {
                    auto relativeWithinRenamedDirectory = std::filesystem::relative(entry.path(), fullOldPath);
                    auto remappedPath = AssetPath(
                        std::filesystem::relative(fullNewPath / relativeWithinRenamedDirectory, GetAssetDirectory())
                    );
                    movedAssets.emplace_back(assetData, remappedPath);
                }
            }
        }
    }
    else if (std::filesystem::is_regular_file(fullOldPath) && !IsMetaFile(fullOldPath))
    {
        AssetData* assetData =
            GetAssetData(oldPath); // at this point old path must be a relative path in Assets directory
        if (assetData != nullptr)
        {
            movedAssets.emplace_back(assetData, newPath);
        }
    }
    else
        return; // anything else return

    std::error_code renameErrorCode;
    std::filesystem::rename(fullOldPath, fullNewPath, renameErrorCode);

    // move failed
    if (renameErrorCode)
        return;

    if (std::filesystem::is_regular_file(fullNewPath))
    {
        std::error_code metaRenameError;
        auto oldMetaPath = GetMetaPath(fullOldPath);
        auto newMetaPath = GetMetaPath(fullNewPath);
        if (std::filesystem::exists(oldMetaPath))
        {
            std::filesystem::rename(oldMetaPath, newMetaPath, metaRenameError);
            if (metaRenameError)
            {
                spdlog::warn("failed to rename asset meta from {} to {}: {}", oldMetaPath.string(), newMetaPath.string(), metaRenameError.message());
            }
        }
    }

    // change assetData information
    for (auto& [assetData, remappedPath] : movedAssets)
    {
        auto oldStoredPath = assetData->GetAssetPath();
        byPath.erase(oldStoredPath);
        assetData->SetAssetPath(remappedPath);
        byPath[remappedPath] = assetData;

        assetData->SaveToDisk(GetProjectRoot());
    }
}

void AssetFileSystem::Remove(const AssetPath& path)
{
    auto fullPath = GetAssetDirectory() / path.ToFilesystemPath();

    if (!std::filesystem::exists(fullPath))
        return;

    auto RemoveAsset = [&](const std::filesystem::path& path)
    {
        auto assetPath = AssetPath(std::filesystem::relative(path, GetAssetDirectory()));

        auto assetData = GetAssetData(assetPath);

        if (assetData)
        {
            // set assetData's imported file to nothing (effectly remove all imported assets)
            SyncImportedAssetFiles(assetData, {});
        }

        std::filesystem::remove(GetMetaPath(path));
    };

    if (std::filesystem::is_directory(fullPath))
    {
        for (auto entry : std::filesystem::recursive_directory_iterator(fullPath))
        {
            if (entry.is_regular_file() && !IsMetaFile(entry.path()))
            {
                RemoveAsset(entry.path());
            }
        }
    }
    else
    {
        RemoveAsset(fullPath);
    }

    std::filesystem::remove_all(fullPath);
}

void AssetFileSystem::RemoveAssetData(AssetData* assetData)
{
    for (auto iter = byPath.begin(); iter != byPath.end();)
    {
        if (iter->second == assetData)
        {
            iter = byPath.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    for (auto iter = byUUID.begin(); iter != byUUID.end();)
    {
        if (iter->second == assetData)
        {
            iter = byUUID.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
}

void AssetFileSystem::UnloadAsset(Asset& asset)
{
    const UUID& uuid = asset.GetUUID();
    auto byUUIDIter = byUUID.find(uuid);
    if (byUUIDIter != byUUID.end())
    {
        AssetData* ptr = byUUIDIter->second;
        ptr->asset = nullptr;
    }
}

void AssetFileSystem::SyncImportedAssetFiles(
    AssetData* assetData, const std::vector<AssetPath>& newImported
)
{
    auto importedAssetPaths = assetData->GetImportedAssetPaths();
    assetData->SetImportedAssetPaths(newImported);

    std::vector<AssetPath> toRemove;
    for (auto& oldp : importedAssetPaths)
    {
        auto findResult = std::find(newImported.begin(), newImported.end(), oldp);
        if (findResult == newImported.end())
        {
            toRemove.push_back(oldp);
        }
    }

    for (auto r : toRemove)
    {
        std::error_code e;
        std::filesystem::remove(r.ToFilesystemPath(), e);
        if (e.value() != 0)
        {
            spdlog::error("failed to remove {}", e.message());
        }
    }
}
