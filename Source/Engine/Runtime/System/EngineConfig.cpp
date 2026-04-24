#include "EngineConfig.hpp"
#include <algorithm>

std::filesystem::path EngineConfig::projectRoot = "";
std::filesystem::path EngineConfig::engineAssetsRoot = std::filesystem::current_path() / "Assets";
std::filesystem::path EngineConfig::projectAssetsRoot = "";

void EngineConfig::SetProjectRoot(const std::filesystem::path& root)
{
    projectRoot = std::filesystem::absolute(root);
    projectAssetsRoot = projectRoot / "Assets";
}

const std::filesystem::path& EngineConfig::GetProjectRoot()
{
    return projectRoot;
}

const std::filesystem::path& EngineConfig::GetEngineAssetsRoot()
{
    return engineAssetsRoot;
}

const std::filesystem::path& EngineConfig::GetProjectAssetsRoot()
{
    return projectAssetsRoot;
}
