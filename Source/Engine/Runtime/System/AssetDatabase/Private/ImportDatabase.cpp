#include "ImportDatabase.hpp"
#include <fmt/format.h>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string_view>

namespace
{
bool ExecSql(sqlite3* db, const char* sql)
{
    char* errMsg = nullptr;
    int result = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (result != SQLITE_OK)
    {
        spdlog::error("sqlite exec failed: {}", errMsg ? errMsg : "unknown error");
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

sqlite3_stmt* Prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* stmt = nullptr;
    int result = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (result != SQLITE_OK)
    {
        spdlog::error("sqlite prepare failed: {}", sqlite3_errmsg(db));
        return nullptr;
    }

    return stmt;
}

bool ColumnExists(sqlite3* db, const char* tableName, const char* columnName)
{
    std::string sql = fmt::format("PRAGMA table_info({});", tableName);
    sqlite3_stmt* stmt = Prepare(db, sql.c_str());
    if (stmt == nullptr)
    {
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (name != nullptr && std::string_view(name) == columnName)
        {
            found = true;
            break;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}
} // namespace

ImportDatabase::~ImportDatabase()
{
    if (db != nullptr)
    {
        sqlite3_close(db);
        db = nullptr;
    }
}

void ImportDatabase::Init(const std::filesystem::path& importDatabaseRoot)
{
    this->importDatabaseRoot = importDatabaseRoot;
    std::filesystem::create_directories(importDatabaseRoot);

    if (db != nullptr)
    {
        sqlite3_close(db);
        db = nullptr;
    }

    auto dbPath = (importDatabaseRoot / "imports.db").string();
    int result = sqlite3_open(dbPath.c_str(), &db);
    if (result != SQLITE_OK)
    {
        spdlog::error("failed to open import database {}: {}", dbPath, sqlite3_errmsg(db));
        return;
    }

    ExecSql(db, "PRAGMA foreign_keys = ON;");
    CreateSchema();
}

bool ImportDatabase::TryGetImportState(const std::string& assetUUID, ImportState& state) const
{
    if (db == nullptr)
    {
        return false;
    }

    sqlite3_stmt* stmt = Prepare(
        db,
        "SELECT source_write_time, meta_hash, content_hash FROM imports WHERE asset_uuid = ?1;"
    );
    if (stmt == nullptr)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW)
    {
        state.sourceWriteTime = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        state.metaHash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        state.contentHash = static_cast<uint64_t>(sqlite3_column_int64(stmt, 2));
        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

void ImportDatabase::UpsertImportState(const std::string& assetUUID, const ImportState& state) const
{
    if (db == nullptr)
    {
        return;
    }

    sqlite3_stmt* stmt = Prepare(
        db,
        "INSERT INTO imports (asset_uuid, source_write_time, meta_hash, content_hash) VALUES (?1, ?2, ?3, ?4) "
        "ON CONFLICT(asset_uuid) DO UPDATE SET source_write_time = excluded.source_write_time, meta_hash = excluded.meta_hash, content_hash = excluded.content_hash;"
    );
    if (stmt == nullptr)
    {
        return;
    }

    sqlite3_bind_text(stmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(state.sourceWriteTime));
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(state.metaHash));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(state.contentHash));

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        spdlog::error("failed to upsert import state for {}: {}", assetUUID, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

bool ImportDatabase::TryGetArtifactPath(
    const std::string& assetUUID, std::string_view kind, std::filesystem::path& relativePath
) const
{
    if (db == nullptr)
    {
        return false;
    }

    sqlite3_stmt* stmt = Prepare(
        db,
        "SELECT relative_path FROM artifacts WHERE asset_uuid = ?1 AND kind = ?2;"
    );
    if (stmt == nullptr)
    {
        return false;
    }

    sqlite3_bind_text(stmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, kind.data(), static_cast<int>(kind.size()), SQLITE_TRANSIENT);

    int step = sqlite3_step(stmt);
    if (step == SQLITE_ROW)
    {
        relativePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    return false;
}

void ImportDatabase::ReplaceArtifact(
    const std::string& assetUUID, std::string_view kind, const std::filesystem::path& relativePath
) const
{
    if (db == nullptr)
    {
        return;
    }

    sqlite3_stmt* stmt = Prepare(
        db,
        "INSERT INTO artifacts (asset_uuid, kind, relative_path) VALUES (?1, ?2, ?3) "
        "ON CONFLICT(asset_uuid, kind) DO UPDATE SET relative_path = excluded.relative_path;"
    );
    if (stmt == nullptr)
    {
        return;
    }

    auto relativePathStr = relativePath.generic_string();
    sqlite3_bind_text(stmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, kind.data(), static_cast<int>(kind.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, relativePathStr.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        spdlog::error("failed to replace artifact for {}: {}", assetUUID, sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
}

void ImportDatabase::DeleteAssetRows(const std::string& assetUUID) const
{
    if (db == nullptr)
    {
        return;
    }

    sqlite3_stmt* readArtifacts = Prepare(db, "SELECT relative_path FROM artifacts WHERE asset_uuid = ?1;");
    if (readArtifacts != nullptr)
    {
        sqlite3_bind_text(readArtifacts, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(readArtifacts) == SQLITE_ROW)
        {
            auto relativePath = reinterpret_cast<const char*>(sqlite3_column_text(readArtifacts, 0));
            if (relativePath != nullptr)
            {
                std::error_code removeError;
                std::filesystem::remove(importDatabaseRoot / relativePath, removeError);
                if (removeError && removeError != std::errc::no_such_file_or_directory)
                {
                    spdlog::warn("failed to remove import artifact {}: {}", relativePath, removeError.message());
                }
            }
        }
        sqlite3_finalize(readArtifacts);
    }

    sqlite3_stmt* artifactStmt = Prepare(db, "DELETE FROM artifacts WHERE asset_uuid = ?1;");
    if (artifactStmt != nullptr)
    {
        sqlite3_bind_text(artifactStmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(artifactStmt) != SQLITE_DONE)
        {
            spdlog::error("failed to delete artifact rows for {}: {}", assetUUID, sqlite3_errmsg(db));
        }
        sqlite3_finalize(artifactStmt);
    }

    sqlite3_stmt* importStmt = Prepare(db, "DELETE FROM imports WHERE asset_uuid = ?1;");
    if (importStmt != nullptr)
    {
        sqlite3_bind_text(importStmt, 1, assetUUID.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(importStmt) != SQLITE_DONE)
        {
            spdlog::error("failed to delete import state for {}: {}", assetUUID, sqlite3_errmsg(db));
        }
        sqlite3_finalize(importStmt);
    }
}

PodVector<uint8_t> ImportDatabase::ReadArtifactFile(const std::filesystem::path& relativePath) const
{
    std::ifstream f(importDatabaseRoot / relativePath, std::ios::binary);
    if (!f.good())
    {
        return {};
    }

    auto absoluteAssetPath = importDatabaseRoot / relativePath;
    auto fileSize = std::filesystem::file_size(absoluteAssetPath);
    PodVector<uint8_t> d(fileSize);
    f.read(reinterpret_cast<char*>(d.data()), static_cast<std::streamsize>(fileSize));
    return d;
}

bool ImportDatabase::ArtifactExists(const std::filesystem::path& relativePath) const
{
    return std::filesystem::exists(importDatabaseRoot / relativePath);
}

std::filesystem::path ImportDatabase::GetImportAssetPath(const std::string& filename) const
{
    return filename;
}

void ImportDatabase::CreateSchema() const
{
    if (db == nullptr)
    {
        return;
    }

    ExecSql(
        db,
        "CREATE TABLE IF NOT EXISTS imports ("
        "asset_uuid TEXT PRIMARY KEY,"
        "source_write_time INTEGER NOT NULL,"
        "meta_hash INTEGER NOT NULL,"
        "content_hash INTEGER NOT NULL DEFAULT 0"
        ");"
    );

    if (!ColumnExists(db, "imports", "content_hash"))
    {
        ExecSql(db, "ALTER TABLE imports ADD COLUMN content_hash INTEGER NOT NULL DEFAULT 0;");
    }

    ExecSql(
        db,
        "CREATE TABLE IF NOT EXISTS artifacts ("
        "asset_uuid TEXT NOT NULL,"
        "kind TEXT NOT NULL,"
        "relative_path TEXT NOT NULL,"
        "PRIMARY KEY (asset_uuid, kind),"
        "FOREIGN KEY (asset_uuid) REFERENCES imports(asset_uuid) ON DELETE CASCADE"
        ");"
    );
}
