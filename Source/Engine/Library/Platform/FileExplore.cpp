#include "FileExplore.hpp"


void Platform::FileExplore::OpenFolder(const std::filesystem::path& path)
{
    if (std::filesystem::is_directory(path))
    {
        std::string cmd = "start " + path.string();
        std::system(cmd.c_str());
    }
}
