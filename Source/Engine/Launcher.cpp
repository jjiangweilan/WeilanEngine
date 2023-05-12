#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Editor/GameEditor.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderGraph/RenderGraph.hpp"
#include <filesystem>
#include <spdlog/spdlog.h>

namespace Engine
{

class WeilanEngie
{
public:
    struct CreateInfo
    {
        std::filesystem::path projectPath;
    };

    WeilanEngie() {}

    void Init(const CreateInfo& createInfo)
    {
        Gfx::GfxDriver::CreateInfo gfxCreateInfo{{1920, 1080}};
        Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);
        gfxDriver = GetGfxDriver().Get();

        assetDatabase = std::make_unique<AssetDatabase>(createInfo.projectPath);
        renderGraph = std::make_unique<RenderGraph>(assetDatabase);
        gameSceneManager = std::make_unique<GameSceneManager>();
        gameEditor = std::make_unique<Editor::GameEditor>(assetDatabase, gameSceneManager);
    }

    void Loop()
    {
        while (true)
        {}
    }

private:
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<Editor::GameEditor> gameEditor;
    std::unique_ptr<RenderGraph> renderGraph;
    std::unique_ptr<GameSceneManager> gameSceneManager;
    Gfx::GfxDriver* gfxDriver;

#if GAME_EDITOR

#endif
};

class Launcher
{
    using ArgList = std::vector<std::string_view>;

public:
    Launcher() {}

    void ArgsParser(int argc, char** argv)
    {
        argList = std::make_unique<ArgList>(argv + 1, argv + argc);
        for (int i = 0; i < argList->size(); ++i)
        {
            DispatchArgs(*argList, i);
        }

        if (!hasAction)
        {
            SPDLOG_ERROR("No action taken, maybe you should set a project path using --project");
        }
    }

    void DispatchArgs(ArgList& args, int& curr)
    {
        if (args[curr] == "--project" || args[curr] == "-p")
        {
            curr++;
            std::filesystem::path path(args[curr]);
            if (std::filesystem::exists(path))
            {
                OpenProject(path);
            }
            else
            {
                SPDLOG_ERROR("{} is not a valid path", path.string());
            }

            hasAction = true;
        }
    }

    void OpenProject(const std::filesystem::path& path) {}

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

} // namespace Engine
  //
  //

int main(int argc, char** argv)
{
    Engine::Launcher l;
    l.ArgsParser(argc, argv);
}
