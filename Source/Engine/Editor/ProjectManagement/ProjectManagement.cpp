#include "ProjectManagement.hpp"
#include "Core/GameScene/GameScene.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/GameScene/GameSceneManager.hpp"

#include <fstream>
#include <cstdlib>
namespace Engine::Editor
{
    std::filesystem::path GetSysConfigPath()
    {
        std::filesystem::path sysConfigDirectory;
#if defined(_WIN32)
        sysConfigDirectory = std::getenv("LOCALAPPDATA");
#elif defined(__APPLE__)
        sysConfigDirectory = std::getenv("HOME");
        sysConfigDirectory /= ".config"; 
#endif
            return sysConfigDirectory / "WeilanEngine";
    }

    ProjectManagement::ResultCode ProjectManagement::CreateNewProject(const std::filesystem::path& path)
    {
        if (std::filesystem::exists(path))
        {
            gameProj = nlohmann::json();
            gameProj["path"] = path;
            std::fstream out;
            out.open(path / "GameProj.json", std::ios::out | std::ios::trunc);
            out << gameProj.dump();
            out.close();
            InitializeProject(path);
            return ResultCode::Success;
        }
        else
            return ResultCode::FilePathError;
    }

    std::filesystem::path ProjectManagement::GetInternalRootPath()
    {
#if ENGINE_DEV_BUILD
        return ENGINE_SOURCE_PATH;
#endif
        return GetSysConfigPath();
    }

    std::vector<std::filesystem::path> ProjectManagement::GetProjectLists()
    {
        std::filesystem::path sysConfigDirectory = GetSysConfigPath();
        if (std::filesystem::exists(sysConfigDirectory))
        {
            std::filesystem::path engineJson = sysConfigDirectory / "engine.json";
            if (std::filesystem::exists(engineJson))
            {
                std::fstream f;
                f.open(engineJson);
                nlohmann::json j = nlohmann::json::parse(f);
                std::filesystem::path lastProjectPath = j["lastProjectPath"];
                return {lastProjectPath};
            }
        }

        return {};
    };

    void ProjectManagement::Save()
    {
        std::fstream out;
        out.open("GameProj.json", std::ios::out | std::ios::trunc);
        out << gameProj.dump();
        out.close();
    }

    ProjectManagement::ResultCode ProjectManagement::LoadProject(const std::filesystem::path& root)
    {
        auto projConfigPath = root / "GameProj.json";
        if (std::filesystem::exists(projConfigPath))
        {
            std::fstream f;
            f.open(projConfigPath);
            if (f.is_open() && f.good())
            {
                gameProj = nlohmann::json::parse(f);
                InitializeProject(gameProj["path"].get<std::string>());
                return ResultCode::Success;
            }
            else
                return ResultCode::FilePathError;
        }
        else
            return ResultCode::FilePathError;
    }

    void ProjectManagement::InitializeProject(const std::filesystem::path& root)
    {
        if (!std::filesystem::exists(root / "Assets")) std::filesystem::create_directory(root / "Assets");
        if (!std::filesystem::exists(root / "Library")) std::filesystem::create_directory(root / "Library");

        std::filesystem::path imGuiDefaultIniPath(ENGINE_SOURCE_PATH);
        imGuiDefaultIniPath /= "Resources/imgui.ini";

        auto imGuiIniPath = root / "imgui.ini";
        if (!std::filesystem::exists(imGuiIniPath))
            std::filesystem::copy_file(imGuiDefaultIniPath, imGuiIniPath);

        std::filesystem::current_path(root);

        // record project path to disk
        auto sysConfigPath = GetSysConfigPath();
        if (std::filesystem::exists(sysConfigPath))
        {
            auto engineConfigPath = sysConfigPath;
            if (!std::filesystem::exists(engineConfigPath)) std::filesystem::create_directory(engineConfigPath);

            std::filesystem::path engineJsonPath = engineConfigPath / "engine.json";
            nlohmann::json engineJson;
            // test if engine.json already exists if so read it and use it
            if (std::filesystem::exists(engineJsonPath))
            {
                std::fstream f;
                f.open(engineJsonPath, std::ios::in);
                engineJson = nlohmann::json::parse(f);
            }
            engineJson["lastProjectPath"] = root;
            std::fstream f;
            f.open(engineJsonPath, std::ios::out | std::ios::trunc);
            f << engineJson.dump();
            f.close();
        }

        initialized = true;
    }

    void ProjectManagement::SetLastActiveScene(RefPtr<GameScene> scene)
    {
        gameProj["lastActiveScene"] = scene->GetUUID().ToString();
    }

    RefPtr<ProjectManagement> ProjectManagement::instance = nullptr;
}
