#pragma once
#include "Library/PodVector.hpp"
#include <cinttypes>
#include <filesystem>
class ImportDatabase
{
public:
    void Init(const std::filesystem::path& importDatabaseRoot) { this->importDatabaseRoot = importDatabaseRoot; }
    PodVector<uint8_t> ReadFile(const std::string& filename) const;

    std::filesystem::path GetImportAssetPath(const std::string& filename) const;
    bool ExistImportFile(std::string_view file) const { return std::filesystem::exists(importDatabaseRoot / file); }
    const std::filesystem::path& GetImportDatabaseRootPath() const { return importDatabaseRoot; };
private:
    const size_t streamBufSize = 1024 * 1024;
    PodVector<char> streamBuf = PodVector<char>(streamBufSize); // LTS for multithreading?
    std::filesystem::path importDatabaseRoot;
};
