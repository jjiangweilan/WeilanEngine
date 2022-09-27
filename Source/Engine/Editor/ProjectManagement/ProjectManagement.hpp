#pragma once

#include "Code/Ptr.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>
namespace Engine
{
    class GameScene;
}
namespace Engine::Editor
{

    std::filesystem::path GetSysConfigPath();
    class ProjectManagement
    {
        public:
            enum class ResultCode
            {
                Success,
                FilePathError
            };

            ResultCode CreateNewProject(const std::filesystem::path& path);
            bool IsInitialized() { return initialized; }
            void RecoverLastProject();
            std::filesystem::path& GetInternalAssetPath();
            void SetLastActiveScene(RefPtr<GameScene> scene);
            void Save();
            ProjectManagement::ResultCode LoadProject();

            static RefPtr<ProjectManagement> instance;

        private:
            void InitializeProjectManagement(const std::filesystem::path& root);
            bool initialized = false;
            nlohmann::json gameProj;

    };
}
