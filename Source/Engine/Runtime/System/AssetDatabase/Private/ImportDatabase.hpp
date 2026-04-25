#pragma once
#include "Engine/Library/PodVector.hpp"
#include <sqlite3.h>
#include <cinttypes>
#include <filesystem>
#include <string_view>

class ImportDatabase
{
public:
    struct ImportState
    {
        uint64_t sourceWriteTime = 0;
        uint64_t metaHash = 0;
    };

    ImportDatabase() = default;
    ~ImportDatabase();

    void Init(const std::filesystem::path& importDatabaseRoot);

    bool TryGetImportState(const std::string& assetUUID, ImportState& state) const;
    void UpsertImportState(const std::string& assetUUID, const ImportState& state) const;

    bool TryGetArtifactPath(const std::string& assetUUID, std::string_view kind, std::filesystem::path& relativePath) const;
    void ReplaceArtifact(const std::string& assetUUID, std::string_view kind, const std::filesystem::path& relativePath) const;
    void DeleteAssetRows(const std::string& assetUUID) const;

    PodVector<uint8_t> ReadArtifactFile(const std::filesystem::path& relativePath) const;
    bool ArtifactExists(const std::filesystem::path& relativePath) const;

    PodVector<uint8_t> ReadFile(const std::string& filename) const { return ReadArtifactFile(filename); }

    std::filesystem::path GetImportAssetPath(const std::string& filename) const;
    bool ExistImportFile(std::string_view file) const { return ArtifactExists(std::filesystem::path(file)); }
    const std::filesystem::path& GetImportDatabaseRootPath() const { return importDatabaseRoot; };

private:
    void CreateSchema() const;

    std::filesystem::path importDatabaseRoot;
    sqlite3* db = nullptr;
};
