#pragma once

#include <filesystem>

namespace Engine
{
    class ProjectConfig
    {
        public:
            void SetProjectRootPath(const std::filesystem::path& rootPath) { projectRootPath = rootPath; }
            const std::filesystem::path& GetProjectRootPath() { return projectRootPath; }

        private:
            std::filesystem::path projectRootPath;
    };
}
