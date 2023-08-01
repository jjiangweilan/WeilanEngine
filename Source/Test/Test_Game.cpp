#include "AssetDatabase/Importers.hpp"
#include "Editor/Tool.hpp"
#include "Rendering/SceneRenderer.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <gtest/gtest.h>
using namespace Engine;
TEST(Gameplay, Test0)
{
    auto engine = std::make_unique<Engine::WeilanEngine>();
    engine->Init({});

    auto sceneRenderer = std::make_unique<Engine::SceneRenderer>();
    Engine::Scene scene({.render = [&sceneRenderer](Engine::Gfx::CommandBuffer& cmd) { sceneRenderer->Execute(cmd); }});

    engine->sceneManager->SetActiveScene(scene);

    sceneRenderer->BuildGraph(
        scene,
        {
            .finalImage = *GetGfxDriver()->GetSwapChainImageProxy(),
            .layout = Gfx::ImageLayout::Present_Src_Khr,
            .accessFlags = Gfx::AccessMask::None,
            .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
        }

    );
    sceneRenderer->Process();

    // set camera
    Engine::GameObject* gameObject = scene.CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Engine::Camera>();
    scene.SetMainCamera(cam);

    // handle swapchain change
    scene.RegisterSystemEventCallback(
        [cam](SDL_Event& event)
        {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                float width = event.window.data1;
                float height = event.window.data2;
                cam->SetProjectionMatrix(glm::radians(45.0f), width / (float)height, 0.01f, 1000.f);
            }
        }
    );

    engine->renderPipeline->RegisterSwapchainRecreateCallback(
        [&sceneRenderer, &scene]()
        {
            sceneRenderer->BuildGraph(
                scene,
                {
                    .finalImage = *GetGfxDriver()->GetSwapChainImageProxy(),
                    .layout = Gfx::ImageLayout::Present_Src_Khr,
                    .accessFlags = Gfx::AccessMask::None,
                    .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
                }
            );
            sceneRenderer->Process();
        }
    );

    // load model
    auto opaqueShader = sceneRenderer->GetOpaqueShader();
    auto model2 = Engine::Importers::GLB("Source/Test/Resources/DamagedHelmet.glb", opaqueShader);
    scene.AddGameObject(model2->GetGameObject()[0].get());
    auto lightGO = scene.CreateGameObject();
    auto light = lightGO->AddComponent<Engine::Light>();
    light->SetIntensity(10);

    engine->Loop();
}
