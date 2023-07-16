#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#if ENGINE_EDITOR
#include "Editor/GameEditor.hpp"
#include "Editor/Renderer.hpp"
#endif
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "Rendering/Shaders.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <filesystem>

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
        shaders = std::make_unique<Rendering::Shaders>();
        assetDatabase = std::make_unique<AssetDatabase>(createInfo.projectPath);
        scene = std::make_unique<Scene>();
#if ENGINE_EDITOR
        gameEditor = std::make_unique<Editor::GameEditor>();
        gameEditorRenderer = std::make_unique<Editor::Renderer>();
#endif

        renderPipeline = std::make_unique<RenderPipeline>(scene.get());
    }

    void LoadScene(const std::filesystem::path& path);

    void Loop()
    {
        while (true)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
#if ENGINE_EDITOR
                ImGui_ImplSDL2_ProcessEvent(&event);
#endif
                if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                {
                    goto LoopEnd;
                }
            }

#if ENGINE_EDITOR
            gameEditor->Tick();
#endif
            // gameEditor->Tick();
            renderPipeline->Render();
        }
    LoopEnd:
        gfxDriver->WaitForIdle();
    }

    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<Rendering::Shaders> shaders;
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<RenderPipeline> renderPipeline;
    std::unique_ptr<Scene> scene;
#if ENGINE_EDITOR
    std::unique_ptr<Editor::Renderer> gameEditorRenderer;
    std::unique_ptr<Editor::GameEditor> gameEditor;
#endif
};
} // namespace Engine
