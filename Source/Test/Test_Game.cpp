#pragma once
#include "AssetDatabase/Importers.hpp"
#include "DualMoonGraph/DualMoonGraph.hpp"
#include "Rendering/Shaders.hpp"
#include "WeilanEngine.hpp"
#include <gtest/gtest.h>

TEST(GamePlay, Test0)
{
    auto engine = std::make_unique<Engine::WeilanEngie>();

    engine->Init({});

    Engine::Shader* shader = engine->shaders->Add("StandardPBR", "Assets/Shaders/Game/StandardPBR.shad");
    auto modualMoonGraph = std::make_unique<Engine::DualMoonGraph>(*engine->scene, *shader);

    auto [swapchainNode, swapchainHandle, depthNode, depthHandle] = modualMoonGraph->GetFinalSwapchainOutputs();

    engine->renderPipeline
        ->SetRenderGraph(std::move(modualMoonGraph), swapchainNode, swapchainHandle, depthNode, depthHandle);

    auto model2 = Engine::Importers::GLB("Source/Test/Resources/Cube.glb", shader);
    engine->scene->AddGameObject(model2->GetGameObject()[0].get());

    // set camera
    Engine::GameObject* gameObject = engine->scene->CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Engine::Camera>();
    engine->scene->SetMainCamera(cam);

    engine->Loop();
}
