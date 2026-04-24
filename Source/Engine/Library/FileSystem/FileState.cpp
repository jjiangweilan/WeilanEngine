#include "FileStat.hpp"
#if __APPLE__
namespace Libs::FileSystem
{
#include <sys/stat.h>
std::time_t GetLastModifiedTime(const std::filesystem::path& path)
{
    struct stat info;
    stat(path.string().c_str(), &info);
    return info.st_mtime;
}
} // namespace Libs::FileSystem
#else

namespace Libs::FileSystem
{
std::time_t GetLastModifiedTime(const std::filesystem::path& path)
{
    return std::chrono::system_clock::to_time_t(
        std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(path)));
}
}; // namespace Libs::FileSystem
#endif
