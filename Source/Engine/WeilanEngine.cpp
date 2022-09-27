#include "WeilanEngine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <spdlog/spdlog.h>
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxFactory.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Rendering/RenderPipeline.hpp"
#include "Core/AssetDatabase/Importers/glbImporter.hpp" 
#include "Core/AssetDatabase/Importers/GeneralImporter.hpp"
#include "Core/AssetDatabase/Importers/ShaderImporter.hpp"
#include "Core/Global/Global.hpp"

// test
#include "Core/Component/Camera.hpp"

namespace Engine
{
    void WeilanEngine::Launch()
    {
        spdlog::set_level(spdlog::level::info);

        Global::InitSingleton();
        AssetDatabase::InitSingleton();
        // register importers
        RegisterAssetImporters();
        // drivers
        vkDriver = MakeUnique<Gfx::VKDriver>();
        vkDriver->Init();

        // modules
        renderPipeline = MakeUnique<Rendering::RenderPipeline>(vkDriver.Get());
#if GAME_EDITOR
        gameEditor = MakeUnique<Editor::GameEditor>(vkDriver.Get());
#endif

        ConfigureProjectPath();
        // init
#if GAME_EDITOR
        AssetDatabase::Instance()->LoadInternalAssets();
        AssetDatabase::Instance()->LoadAllAssets();
        gameEditor->LoadCurrentProject();
#endif
        renderPipeline->Init();
        gameEditor->Init(renderPipeline);

        // main loop
        SDL_Event sdlEvent;
        bool shouldBreak = false;
        auto start = std::chrono::steady_clock::now();
        auto preTime = std::chrono::steady_clock::now();

        while (!shouldBreak)
        {
            auto current = std::chrono::steady_clock::now();
            float time = std::chrono::duration<float>(current - start).count();
            float delta = std::chrono::duration<float>(current - preTime).count();
            preTime = current;
            float speed = delta * 1000;

            while (SDL_PollEvent(&sdlEvent)) {
#if GAME_EDITOR
                gameEditor->ProcessEvent(sdlEvent);
#endif
                switch (sdlEvent.type) {
                    case SDL_KEYDOWN:
                    if (sdlEvent.key.keysym.scancode == SDL_SCANCODE_Q)
                    {
                        shouldBreak = true;
                    }
                }
            }

            auto activeGameScene = GameSceneManager::Instance()->GetActiveGameScene();
            renderPipeline->Render(activeGameScene);

#if GAME_EDITOR
            gameEditor->Tick();
            gameEditor->Render();
#endif
            vkDriver->DispatchGPUWork();
        }

        int i = 0;
    }

    WeilanEngine::~WeilanEngine()
    {
        gameEditor = nullptr;
        renderPipeline = nullptr;
        AssetDatabase::Deinit();
    }

    void WeilanEngine::RegisterAssetImporters()
    {
        AssetImporter::RegisterImporter("glb", []() { return MakeUnique<Internal::glbImporter>(); });
        AssetImporter::RegisterImporter("mat", []() { return MakeUnique<Internal::GeneralImporter<Material>>(); });
        AssetImporter::RegisterImporter("game", []() { return MakeUnique<Internal::GeneralImporter<GameScene>>(); });
        AssetImporter::RegisterImporter("shad", []() { return MakeUnique<Internal::ShaderImporter>(); });
    }

    void WeilanEngine::ConfigureProjectPath()
    {
#if GAME_EDITOR
        gameEditor->ConfigEditorPath();
#endif
    }
}
