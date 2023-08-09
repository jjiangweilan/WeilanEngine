#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Importers.hpp"
#include "Core/Scene/SceneManager.hpp"
#if ENGINE_EDITOR
#include "Editor/GameEditor.hpp"
#include "Editor/Renderer.hpp"
#endif
#include "Core/Time.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/RenderPipeline.hpp"
#include "Rendering/Shaders.hpp"
#include "ThirdParty/imgui/imgui_impl_sdl.h"
#include <filesystem>

namespace Engine
{
class WeilanEngine
{
public:
    struct CreateInfo
    {
        std::filesystem::path projectPath;
    };

    WeilanEngine() {}

    std::unique_ptr<Model2> ImportModel(const char* path)
    {
        return Engine::Importers::GLB(path, sceneRenderer->GetOpaqueShader());
    }

    void Init(const CreateInfo& createInfo)
    {
        Gfx::GfxDriver::CreateInfo gfxCreateInfo{{960, 540}};
        gfxDriver = Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan, gfxCreateInfo);
        assetDatabase = std::make_unique<AssetDatabase>(createInfo.projectPath);
        sceneManager = std::make_unique<SceneManager>();
        renderPipeline = std::make_unique<RenderPipeline>();

#if ENGINE_EDITOR
        gameEditor = std::make_unique<Editor::GameEditor>(*this);
#endif

        sceneRenderer = std::make_unique<Engine::SceneRenderer>();
        sceneRenderer->BuildGraph({
            .finalImage = *GetGfxDriver()->GetSwapChainImage(),
            .layout = Gfx::ImageLayout::Present_Src_Khr,
            .accessFlags = Gfx::AccessMask::None,
            .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
        }

        );
        sceneRenderer->Process();
        renderPipeline->RegisterSwapchainRecreateCallback(
            [this]
            {
                sceneRenderer->BuildGraph({
                    .finalImage = *GetGfxDriver()->GetSwapChainImage(),
                    .layout = Gfx::ImageLayout::Present_Src_Khr,
                    .accessFlags = Gfx::AccessMask::None,
                    .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
                });
                sceneRenderer->Process();
            }
        );

        auto queueFamilyIndex = GetGfxDriver()->GetQueue(QueueType::Main)->GetFamilyIndex();
        cmdPool = gfxDriver->CreateCommandPool({queueFamilyIndex});
        cmd = cmdPool->AllocateCommandBuffers(Gfx::CommandBufferType::Primary, 1)[0];
    }

    void LoadScene(const std::filesystem::path& path){};

    void Loop()
    {
        while (true)
        {
            auto scene = sceneManager->GetActiveScene();

            Time::Tick();
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
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    gameEditor->OnWindowResize(event.window.data1, event.window.data2);
                }

                if (scene)
                {
                    scene->InvokeSystemEventCallbacks(event);
                }
            }

            if (scene)
            {
                scene->Tick();
            }
#if ENGINE_EDITOR
            gameEditor->Tick();
#endif

            renderPipeline->WaitForPreviousFrame();

            Rendering::CmdSubmitGroup cmdSubmitGroup;
            if (scene)
            {
                cmdPool->ResetCommandPool();
                cmd->Begin();
                sceneRenderer->Render(*cmd, *scene);
                cmd->End();

                cmdSubmitGroup.AddCmd(cmd.get());
            }
#if ENGINE_EDITOR
            cmdSubmitGroup.AppendToBack(gameEditor->Render());
#endif

            renderPipeline->Render(cmdSubmitGroup);
        }
    LoopEnd:
        gfxDriver->WaitForIdle();
    }

    std::vector<std::function<void(SDL_Event& event)>> eventCallback;

    std::unique_ptr<Gfx::GfxDriver> gfxDriver;
    std::unique_ptr<AssetDatabase> assetDatabase;
    std::unique_ptr<RenderPipeline> renderPipeline;
    std::unique_ptr<SceneRenderer> sceneRenderer;
    std::unique_ptr<SceneManager> sceneManager;
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::unique_ptr<Gfx::CommandPool> cmdPool;
#if ENGINE_EDITOR
    std::unique_ptr<Editor::GameEditor> gameEditor;
#endif
};
} // namespace Engine
