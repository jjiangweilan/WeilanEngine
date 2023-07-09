#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Editor/GameEditor.hpp"
#include "Editor/Renderer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <filesystem>
#include <iostream>
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
        Gfx::GfxDriver::CreateInfo gfxCreateInfo{{960, 540}};
        gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);

        assetDatabase = std::make_unique<AssetDatabase>(createInfo.projectPath);
        // renderGraph = std::make_unique<RenderGraph>(assetDatabase);
        gameSceneManager = std::make_unique<GameSceneManager>();
#if WE_EDITOR
        gameEditor = std::make_unique<Editor::GameEditor>();
        gameEditorRenderer = std::make_unique<Editor::Renderer>();
#endif
        renderPipeline = std::make_unique<RenderPipeline>();
    }

    void Loop()
    {
        while (true)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
#if WE_EDITOR
                ImGui_ImplSDL2_ProcessEvent(&event);
#endif
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                {
                    goto LoopEnd;
                }
            }

#if WE_EDITOR
            gameEditor->Tick();
#endif
            // gameEditor->Tick();
            renderPipeline->Render();
        }
LoopEnd:
        gfxDriver->WaitForIdle();
    }
    
private:
    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<RenderPipeline> renderPipeline;
    std::unique_ptr<GameSceneManager> gameSceneManager;
#if WE_EDITOR
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
    std::unique_ptr<Editor::GameEditor> gameEditor;
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
            // if (std::filesystem::exists(path))
            // {}
            // else
            // {
            //     SPDLOG_ERROR("{} is not a valid path", path.string());
            // }

            auto engine = std::make_unique<WeilanEngie>();
            engine->Init({path});
            engine->Loop();

            hasAction = true;
        }
    }

    std::unique_ptr<ArgList> argList;
    bool hasAction = false;
};

} // namespace Engine
#undef main

int main(int argc, char** argv)
{
    Engine::Launcher l;
    l.ArgsParser(argc, argv);
}
