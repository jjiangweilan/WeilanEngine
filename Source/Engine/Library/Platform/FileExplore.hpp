#pragma once
#include <filesystem>

namespace Platform
{
class FileExplore
{
public:
    static void OpenFolder(const std::filesystem::path& path);
};
} // namespace Platform
