#pragma once
#include <filesystem>
#include <time.h>

namespace Libs::FileSystem
{
std::time_t GetLastModifiedTime(const std::filesystem::path& path);
} // namespace Libs::FileSystem
