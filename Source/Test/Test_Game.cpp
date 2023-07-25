#pragma once
#include "AssetDatabase/Importers.hpp"
#include "Rendering/DualMoonRenderer.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <gtest/gtest.h>

TEST(Gameplay, Test0)
{
    auto engine = std::make_unique<Engine::WeilanEngine>();
    engine->Init({});

    auto sceneRenderer = std::make_unique<Engine::DualMoonRenderer>();
    Engine::Scene scene({.render = [&sceneRenderer](Engine::Gfx::CommandBuffer& cmd) { sceneRenderer->Execute(cmd); }});

    engine->sceneManager->SetActiveScene(scene);

    sceneRenderer->BuildGraph(scene);
    auto [colorNode, colorHandle, depthNode, depthHandle] = sceneRenderer->GetFinalSwapchainOutputs();
    ((Engine::RenderGraph::Graph*)sceneRenderer.get())->Process(colorNode, colorHandle);

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
            sceneRenderer->BuildGraph(scene);
            auto [colorNode, colorHandle, depthNode, depthHandle] = sceneRenderer->GetFinalSwapchainOutputs();
            ((Engine::RenderGraph::Graph*)sceneRenderer.get())->Process(colorNode, colorHandle);
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
