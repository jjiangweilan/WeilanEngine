#pragma once
#include "AssetDatabase/Importers.hpp"
#include "DualMoonGraph/DualMoonGraph.hpp"
#include "Rendering/Shaders.hpp"
#include "WeilanEngine.hpp"
#include "spdlog/spdlog.h"
#include <gtest/gtest.h>

TEST(GamePlay, Test0)
{
    auto engine = std::make_unique<Engine::WeilanEngie>();

    engine->Init({});
    Engine::Rendering::Shaders shaders;

    // set camera
    Engine::GameObject* gameObject = engine->scene->CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Engine::Camera>();
    engine->scene->SetMainCamera(cam);

    auto opaqueShader = shaders.Add("StandardPBR", "Assets/Shaders/Game/StandardPBR.shad");
    auto shadowShader = shaders.Add("ShadowMap", "Assets/Shaders/Game/ShadowMap.shad");

    auto dualMoonGraph = std::make_unique<Engine::DualMoonGraph>(*engine->scene, *opaqueShader, *shadowShader);
    auto [swapchainNode, swapchainHandle, depthNode, depthHandle] = dualMoonGraph->GetFinalSwapchainOutputs();
    engine->renderPipeline->SetRenderGraph(dualMoonGraph.get(), swapchainNode, swapchainHandle, depthNode, depthHandle);

    engine->renderPipeline->RegisterSwapchainRecreateCallback(
        [&engine, &dualMoonGraph, opaqueShader, shadowShader, cam]()
        {
            dualMoonGraph = std::make_unique<Engine::DualMoonGraph>(*engine->scene, *opaqueShader, *shadowShader);
            auto [swapchainNode, swapchainHandle, depthNode, depthHandle] = dualMoonGraph->GetFinalSwapchainOutputs();
            engine->renderPipeline
                ->SetRenderGraph(dualMoonGraph.get(), swapchainNode, swapchainHandle, depthNode, depthHandle);

            auto swapChainImage = Engine::GetGfxDriver()->GetSwapChainImageProxy();
            auto& desc = swapChainImage->GetDescription();
            cam->SetProjectionMatrix(glm::radians(45.0f), desc.width / (float)desc.height, 0.01f, 1000.f);
        }
    );

    auto model2 = Engine::Importers::GLB("Source/Test/Resources/DamagedHelmet.glb", opaqueShader);
    engine->scene->AddGameObject(model2->GetGameObject()[0].get());

    auto lightGO = engine->scene->CreateGameObject();
    auto light = lightGO->AddComponent<Engine::Light>();
    light->SetIntensity(10);

    engine->Loop();
}
