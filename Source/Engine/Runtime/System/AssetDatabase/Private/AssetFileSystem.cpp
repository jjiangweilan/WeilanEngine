#include "AssetFileSystem.hpp"
#include <algorithm>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

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

bool StartsOutsideAssetDirectory(const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return true;
    }

    const std::filesystem::path normalized = path.lexically_normal();
    return !normalized.empty() && *normalized.begin() == "..";
}

bool IsSameOrDescendant(const std::filesystem::path& path, const std::filesystem::path& parent)
{
    const std::filesystem::path relative = path.lexically_normal().lexically_relative(parent.lexically_normal());
    return relative.empty() || (!relative.is_absolute() && *relative.begin() != "..");
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

bool AssetFileSystem::Rename(const AssetPath& oldPath, const AssetPath& newPath, std::string& error)
{
    error.clear();
    if (oldPath == newPath)
        return true;

    if (oldPath.empty() || oldPath.IsInternal() || newPath.IsInternal() ||
        StartsOutsideAssetDirectory(oldPath.ToFilesystemPath()) ||
        StartsOutsideAssetDirectory(newPath.ToFilesystemPath()))
    {
        error = "Source and destination must be inside the project Assets directory.";
        return false;
    }

    auto fullNewPath = GetAssetDirectory() / newPath.ToFilesystemPath();
    auto fullOldPath = GetAssetDirectory() / oldPath.ToFilesystemPath();
    if (!std::filesystem::is_directory(fullNewPath.parent_path()))
    {
        error = fmt::format("Destination directory '{}' does not exist.", newPath.GetParentPath().string());
        return false;
    }
    if (!std::filesystem::exists(fullOldPath))
    {
        error = fmt::format("Source '{}' does not exist.", oldPath.string());
        return false;
    }
    if (std::filesystem::exists(fullNewPath))
    {
        error = fmt::format("An item named '{}' already exists in the destination.", newPath.GetFileName());
        return false;
    }
    if (std::filesystem::is_directory(fullOldPath) && IsSameOrDescendant(fullNewPath, fullOldPath))
    {
        error = fmt::format("Folder '{}' cannot be moved into itself.", oldPath.GetFileName());
        return false;
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
    {
        error = fmt::format("Source '{}' is not a movable file or folder.", oldPath.string());
        return false;
    }

    std::error_code renameErrorCode;
    std::filesystem::rename(fullOldPath, fullNewPath, renameErrorCode);

    // move failed
    if (renameErrorCode)
    {
        error = fmt::format(
            "Failed to move '{}' to '{}': {}",
            oldPath.string(),
            newPath.string(),
            renameErrorCode.message()
        );
        return false;
    }

    if (std::filesystem::is_regular_file(fullNewPath))
    {
        auto oldMetaPath = GetMetaPath(fullOldPath);
        auto newMetaPath = GetMetaPath(fullNewPath);
        if (std::filesystem::exists(oldMetaPath))
        {
            std::error_code metaRenameError;
            std::filesystem::rename(oldMetaPath, newMetaPath, metaRenameError);
            if (metaRenameError)
            {
                std::error_code rollbackError;
                std::filesystem::rename(fullNewPath, fullOldPath, rollbackError);
                error = fmt::format(
                    "Failed to move metadata for '{}': {}",
                    oldPath.string(),
                    metaRenameError.message()
                );
                if (rollbackError)
                {
                    spdlog::error(
                        "failed to roll back asset move from {} to {}: {}",
                        fullNewPath.string(),
                        fullOldPath.string(),
                        rollbackError.message()
                    );
                    error += " The asset move could not be rolled back; see the log.";
                }
                return false;
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

    return true;
}

bool AssetFileSystem::Move(
    const std::vector<AssetPath>& sources,
    const AssetPath& destinationDirectory,
    std::string& error
)
{
    error.clear();
    if (destinationDirectory.IsInternal() || StartsOutsideAssetDirectory(destinationDirectory.ToFilesystemPath()))
    {
        error = "Destination must be inside the project Assets directory.";
        return false;
    }

    const std::filesystem::path destinationPath =
        GetAssetDirectory() / destinationDirectory.ToFilesystemPath();
    if (!std::filesystem::is_directory(destinationPath))
    {
        error = fmt::format("Destination '{}' is not a folder.", destinationDirectory.string());
        return false;
    }

    std::vector<AssetPath> normalizedSources;
    normalizedSources.reserve(sources.size());
    for (const AssetPath& source : sources)
    {
        if (source.empty() || source.IsInternal() || StartsOutsideAssetDirectory(source.ToFilesystemPath()))
        {
            error = "Every moved item must be inside the project Assets directory.";
            return false;
        }
        if (std::find(normalizedSources.begin(), normalizedSources.end(), source) == normalizedSources.end())
        {
            normalizedSources.push_back(source);
        }
    }

    std::sort(
        normalizedSources.begin(),
        normalizedSources.end(),
        [](const AssetPath& left, const AssetPath& right)
        {
            return left.ToFilesystemPath().lexically_normal().native().size() <
                   right.ToFilesystemPath().lexically_normal().native().size();
        }
    );
    std::vector<AssetPath> topLevelSources;
    topLevelSources.reserve(normalizedSources.size());
    for (const AssetPath& candidate : normalizedSources)
    {
        const bool coveredByParent = std::any_of(
            topLevelSources.begin(),
            topLevelSources.end(),
            [&candidate](const AssetPath& possibleParent)
            {
                return IsSameOrDescendant(
                    candidate.ToFilesystemPath(),
                    possibleParent.ToFilesystemPath()
                );
            }
        );
        if (!coveredByParent)
        {
            topLevelSources.push_back(candidate);
        }
    }
    normalizedSources = std::move(topLevelSources);

    struct PlannedMove
    {
        AssetPath source;
        AssetPath destination;
    };
    std::vector<PlannedMove> plannedMoves;
    plannedMoves.reserve(normalizedSources.size());

    for (const AssetPath& source : normalizedSources)
    {
        const std::filesystem::path sourcePath = GetAssetDirectory() / source.ToFilesystemPath();
        if (!std::filesystem::exists(sourcePath))
        {
            error = fmt::format("Source '{}' does not exist.", source.string());
            return false;
        }

        const AssetPath destination(
            destinationDirectory.ToFilesystemPath() / source.ToFilesystemPath().filename()
        );
        if (source == destination)
        {
            continue;
        }

        const std::filesystem::path fullDestination = GetAssetDirectory() / destination.ToFilesystemPath();
        if (std::filesystem::is_directory(sourcePath) && IsSameOrDescendant(destinationPath, sourcePath))
        {
            error = fmt::format("Folder '{}' cannot be moved into itself.", source.GetFileName());
            return false;
        }
        if (std::filesystem::exists(fullDestination))
        {
            error = fmt::format(
                "An item named '{}' already exists in '{}'.",
                destination.GetFileName(),
                destinationDirectory.string()
            );
            return false;
        }
        if (std::filesystem::exists(GetMetaPath(sourcePath)) &&
            std::filesystem::exists(GetMetaPath(fullDestination)))
        {
            error = fmt::format("Metadata already exists for '{}'.", destination.string());
            return false;
        }
        if (std::any_of(
                plannedMoves.begin(),
                plannedMoves.end(),
                [&destination](const PlannedMove& move) { return move.destination == destination; }
            ))
        {
            error = fmt::format("Multiple selected items would become '{}'.", destination.string());
            return false;
        }
        plannedMoves.push_back({source, destination});
    }

    std::vector<PlannedMove> completedMoves;
    completedMoves.reserve(plannedMoves.size());
    for (const PlannedMove& move : plannedMoves)
    {
        if (Rename(move.source, move.destination, error))
        {
            completedMoves.push_back(move);
            continue;
        }

        for (auto iter = completedMoves.rbegin(); iter != completedMoves.rend(); ++iter)
        {
            std::string rollbackError;
            if (!Rename(iter->destination, iter->source, rollbackError))
            {
                spdlog::error(
                    "failed to roll back asset move from {} to {}: {}",
                    iter->destination.string(),
                    iter->source.string(),
                    rollbackError
                );
                error += " One or more completed moves could not be rolled back; see the log.";
            }
        }
        return false;
    }

    return true;
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
