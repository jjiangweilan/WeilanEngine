#pragma once
#include <filesystem>

class EngineConfig
{
public:
    static void SetProjectRoot(const std::filesystem::path& root);
    static const std::filesystem::path& GetProjectRoot();
    static const std::filesystem::path& GetEngineAssetsRoot();
    static const std::filesystem::path& GetProjectAssetsRoot();

private:
    static std::filesystem::path projectRoot;
    static std::filesystem::path engineAssetsRoot;
    static std::filesystem::path projectAssetsRoot;
};
