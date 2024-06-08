#include "WeilanEngine.hpp"
#include <gtest/gtest.h>
using namespace Engine;
TEST(EnvironmentBaker, Test0)
{
    auto engine = std::make_unique<WeilanEngine>();
    engine->Init({});

    auto sceneRenderer = std::make_unique<SceneRenderer>(*engine->assetDatabase);
    Scene scene;

    sceneRenderer->BuildGraph({
        .finalImage = GetGfxDriver()->GetSwapChainImage(),
        .layout = Gfx::ImageLayout::Present_Src_Khr,
        .accessFlags = Gfx::AccessMask::None,
        .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
    });
    auto [colorNode, colorHandle, depthNode, depthHandle] = sceneRenderer->GetFinalSwapchainOutputs();
    ((RenderGraph::Graph*)sceneRenderer.get())->Process(colorNode, colorHandle);

    // set camera
    GameObject* gameObject = scene.CreateGameObject();
    gameObject->SetName("Camera");
    auto cam = gameObject->AddComponent<Camera>();
    scene.SetMainCamera(cam);

    // handle swapchain change

    // engine->renderPipeline->RegisterSwapchainRecreateCallback(
    //     [&sceneRenderer, &scene]()
    //     {
    //         sceneRenderer->BuildGraph(
    //             {
    //                 .finalImage = *GetGfxDriver()->GetSwapChainImage(),
    //                 .layout = Gfx::ImageLayout::Present_Src_Khr,
    //                 .accessFlags = Gfx::AccessMask::None,
    //                 .stageFlags = Gfx::PipelineStage::Bottom_Of_Pipe,
    //             }
    //         );
    //         auto [colorNode, colorHandle, depthNode, depthHandle] = sceneRenderer->GetFinalSwapchainOutputs();
    //         ((RenderGraph::Graph*)sceneRenderer.get())->Process(colorNode, colorHandle);
    //     }
    // );

    // load model
    // auto opaqueShader = sceneRenderer->GetOpaqueShader();
    // auto model2 = Importers::GLB("Source/Test/Resources/EnvrionmentBake.glb", opaqueShader);
    // scene.AddGameObject(model2->GetGameObject()[0].get());
    // auto lightGO = scene.CreateGameObject();
    // auto light = lightGO->AddComponent<Light>();
    // light->SetIntensity(10);
}
