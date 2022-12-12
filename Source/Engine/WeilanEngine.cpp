#include "WeilanEngine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <spdlog/spdlog.h>
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/Rendering/RenderPipeline.hpp"
#include "Core/AssetDatabase/Importers/glbImporter.hpp" 
#include "Core/AssetDatabase/Importers/GeneralImporter.hpp"
#include "Core/AssetDatabase/Importers/ShaderImporter.hpp"
#include "Core/AssetDatabase/Importers/TextureImporter.hpp"
#include "Script/LuaBackend.hpp"

namespace Engine
{
    void WeilanEngine::Launch()
    {
        // drivers
        Gfx::GfxDriver::CreateGfxDriver(Gfx::Backend::Vulkan);
        gfxDriver = Gfx::GfxDriver::Instance();

        spdlog::set_level(spdlog::level::info);

        projectManagement = MakeUnique<Editor::ProjectManagement>();
        Editor::ProjectManagement::instance = projectManagement;
        auto projectList = projectManagement->GetProjectLists();
        if (!projectList.empty())
        {
            projectManagement->LoadProject(projectList[0]);
        }
        LuaBackend::Instance()->LoadLuaInFolder(projectList[0] / "Assets");

        AssetDatabase::InitSingleton(projectList[0]);
        RegisterAssetImporters();
        AssetDatabase::Instance()->LoadInternalAssets();
        AssetDatabase::Instance()->LoadAllAssets();

        // recover last active scene
        UUID lastActiveSceneUUID = projectManagement->GetLastActiveScene();
        if (!lastActiveSceneUUID.IsEmpty())
            GameSceneManager::Instance()->SetActiveGameScene(AssetDatabase::Instance()->GetAssetObject(lastActiveSceneUUID));

        // modules
        renderPipeline = MakeUnique<Rendering::RenderPipeline>(gfxDriver.Get());
#if GAME_EDITOR
        gameEditor = MakeUnique<Editor::GameEditor>(gfxDriver.Get(), projectManagement);
#endif

        renderPipeline->Init();
        gameEditor->Init(renderPipeline);

        // main loop
        SDL_Event sdlEvent;
        bool shouldBreak = false;
        while (!shouldBreak)
        {

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

            // update
            auto activeGameScene = GameSceneManager::Instance()->GetActiveGameScene();
            if (activeGameScene) activeGameScene->Tick();
#if GAME_EDITOR
            gameEditor->Tick();
#endif

            // rendering
            if (activeGameScene) renderPipeline->Render(activeGameScene);
#if GAME_EDITOR
            gameEditor->Render();
#endif
            gfxDriver->DispatchGPUWork();

            AssetDatabase::Instance()->EndOfFrameUpdate();
        }
    }

    WeilanEngine::~WeilanEngine()
    {
        gameEditor = nullptr;
        renderPipeline = nullptr;
        AssetDatabase::Deinit();
        Gfx::GfxDriver::DestroyGfxDriver();
    }

    void WeilanEngine::RegisterAssetImporters()
    {
        RefPtr<AssetDatabase> assetDb = AssetDatabase::Instance();
        assetDb->RegisterImporter<Internal::glbImporter>("glb");
        assetDb->RegisterImporter<Internal::GeneralImporter<Material>>("mat");
        assetDb->RegisterImporter<Internal::GeneralImporter<Rendering::RenderPipelineAsset>>("rp");
        assetDb->RegisterImporter<Internal::GeneralImporter<GameScene>>("game");
        assetDb->RegisterImporter<Internal::ShaderImporter>("shad");
        assetDb->RegisterImporter<Internal::TextureImporter>("png");
        assetDb->RegisterImporter<Internal::TextureImporter>("jpeg");
        assetDb->RegisterImporter<Internal::TextureImporter>("jpg");
    }
}
