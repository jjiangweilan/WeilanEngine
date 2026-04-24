#pragma once
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>
#include "Engine/Runtime/System/EngineConfig.hpp"

#include <string_view>

class AssetPath
{
    std::string path;
    bool isInternal = false;

    static std::string Normalize(std::string_view p)
    {
        if (p.empty()) return "";

        std::string pathStr;
        std::filesystem::path fsPath(p);

        if (fsPath.is_absolute())
        {
            std::error_code ec;
            // Try project assets first
            auto projectAssets = EngineConfig::GetProjectAssetsRoot();
            if (!projectAssets.empty())
            {
                auto rel = std::filesystem::relative(fsPath, projectAssets, ec);
                if (!ec && !rel.empty() && rel.generic_string().find("..") == std::string::npos)
                {
                    pathStr = rel.generic_string();
                }
            }

            if (pathStr.empty())
            {
                // Try engine assets
                auto engineAssets = EngineConfig::GetEngineAssetsRoot();
                if (!engineAssets.empty())
                {
                    auto rel = std::filesystem::relative(fsPath, engineAssets, ec);
                    if (!ec && !rel.empty() && rel.generic_string().find("..") == std::string::npos)
                    {
                        pathStr = "_engine_internal/" + rel.generic_string();
                    }
                }
            }

            if (pathStr.empty())
            {
                // Both failed, return empty
                return "";
            }
        }
        else
        {
            pathStr = std::string(p);
        }

        std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

        // Remove trailing slashes
        while (!pathStr.empty() && pathStr.back() == '/')
        {
            pathStr.pop_back();
        }

        return pathStr;
    }

    void UpdateInternalStatus()
    {
        isInternal = path.starts_with("_engine_internal/");
    }

public:
    AssetPath() = default;
    AssetPath(const std::string& p) : path(Normalize(p)) { UpdateInternalStatus(); }
    AssetPath(const char* p) : path(Normalize(p)) { UpdateInternalStatus(); }
    AssetPath(std::string_view p) : path(Normalize(p)) { UpdateInternalStatus(); }
    AssetPath(const std::filesystem::path& p) : path(Normalize(p.string())) { UpdateInternalStatus(); }

    const std::string& string() const { return path; }
    const char* c_str() const { return path.c_str(); }

    bool operator==(const AssetPath& other) const { return path == other.path; }
    bool operator!=(const AssetPath& other) const { return path != other.path; }
    bool operator<(const AssetPath& other) const { return path < other.path; }

    bool empty() const { return path.empty(); }

    AssetPath operator/(const AssetPath& other) const
    {
        if (path.empty()) return other;
        if (other.path.empty()) return *this;
        return AssetPath(path + "/" + other.path);
    }

    // Explicitly convert to std::filesystem::path
    std::filesystem::path ToFilesystemPath() const { return std::filesystem::path(path); }

    std::string GetExtension() const
    {
        size_t dotPos = path.find_last_of('.');
        if (dotPos == std::string::npos) return "";
        return path.substr(dotPos);
    }

    std::string GetFileName() const
    {
        size_t slashPos = path.find_last_of('/');
        if (slashPos == std::string::npos) return path;
        return path.substr(slashPos + 1);
    }

    std::string GetFileNameWithoutExtension() const
    {
        std::string fileName = GetFileName();
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos == std::string::npos) return fileName;
        return fileName.substr(0, dotPos);
    }

    AssetPath GetParentPath() const
    {
        size_t slashPos = path.find_last_of('/');
        if (slashPos == std::string::npos) return AssetPath("");
        return AssetPath(path.substr(0, slashPos));
    }

    bool IsInternal() const { return isInternal; }

    std::filesystem::path ToAbsolutePath() const
    {
        if (isInternal)
        {
            // remove _engine_internal/
            std::string realPath = path.substr(17);
            return EngineConfig::GetEngineAssetsRoot() / realPath;
        }
        else
        {
            return EngineConfig::GetProjectAssetsRoot() / path;
        }
    }

    operator std::filesystem::path() const { return ToAbsolutePath(); }

    size_t hash() const { return std::hash<std::string>{}(path); }
};

namespace std
{
    template <>
    struct hash<AssetPath>
    {
        size_t operator()(const AssetPath& p) const { return p.hash(); }
    };
}

namespace boost
{
    template <typename T>
    struct hash;

    template <>
    struct hash<AssetPath>
    {
        size_t operator()(const AssetPath& p) const { return p.hash(); }
    };
}
