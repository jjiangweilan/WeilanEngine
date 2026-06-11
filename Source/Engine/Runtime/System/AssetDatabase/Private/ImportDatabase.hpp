#pragma once
#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/Runtime/System/AssetDatabase/ArtifactTypes.hpp"
#include "Engine/Library/PodVector.hpp"
#include "Engine/Library/UUID.hpp"
#include <sqlite3.h>
#include <cinttypes>
#include <filesystem>
#include <string_view>
#include <vector>

class ImportDatabase
{
public:
    struct ImportState
    {
        uint64_t sourceWriteTime = 0;
        uint64_t metaHash = 0;
        uint64_t contentHash = 0;
    };

    struct ArtifactRecord
    {
        UUID artifactUUID;
        UUID sourceAssetUUID;
        std::string kind;
        std::string name;
        std::filesystem::path relativePath;
        bool isMain = false;
        std::string locator;
    };

    ImportDatabase() = default;
    WEILAN_ENGINE_API ~ImportDatabase();

    WEILAN_ENGINE_API void Init(const std::filesystem::path& importDatabaseRoot);

    WEILAN_ENGINE_API bool TryGetImportState(const std::string& assetUUID, ImportState& state) const;
    WEILAN_ENGINE_API void UpsertImportState(const std::string& assetUUID, const ImportState& state) const;

    bool TryGetArtifactPath(const std::string& assetUUID, std::string_view kind, std::filesystem::path& relativePath) const;
    WEILAN_ENGINE_API bool TryGetArtifactPath(
        const std::string& assetUUID,
        AssetArtifacts::Kind kind,
        std::filesystem::path& relativePath
    ) const;
    bool TryGetArtifactPath(const UUID& artifactUUID, std::filesystem::path& relativePath) const;
    void ReplaceArtifact(const std::string& assetUUID, std::string_view kind, const std::filesystem::path& relativePath) const;
    WEILAN_ENGINE_API void ReplaceArtifact(
        const std::string& assetUUID,
        AssetArtifacts::Kind kind,
        const std::filesystem::path& relativePath
    ) const;
    void ReplaceArtifact(
        const UUID& sourceAssetUUID,
        const UUID& artifactUUID,
        std::string_view kind,
        std::string_view name,
        const std::filesystem::path& relativePath,
        bool isMain,
        std::string_view locator = {}
    ) const;
    void ReplaceArtifact(
        const UUID& sourceAssetUUID,
        const UUID& artifactUUID,
        AssetArtifacts::Kind kind,
        std::string_view name,
        const std::filesystem::path& relativePath,
        bool isMain,
        std::string_view locator = {}
    ) const;
    WEILAN_ENGINE_API std::vector<ArtifactRecord> ListArtifacts(const UUID& sourceAssetUUID) const;
    std::vector<ArtifactRecord> ListArtifacts(const UUID& sourceAssetUUID, std::string_view kind) const;
    std::vector<ArtifactRecord> ListArtifacts(const UUID& sourceAssetUUID, AssetArtifacts::Kind kind) const;
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
