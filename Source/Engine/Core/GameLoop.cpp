#include "GameLoop.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "Scene/RenderingScene.hpp"
#include "Scene/Scene.hpp"
#include <spdlog/spdlog.h>
GameLoop::GameLoop()
{
    cmd = GetGfxDriver()->CreateCommandBuffer();
}

GameLoop::~GameLoop() {}

static void TickGameObject(GameObject* go)
{
    go->Tick();

    for (auto chil : go->GetChildren())
    {
        TickGameObject(chil);
    }
}

const Gfx::RG::ImageIdentifier* GameLoop::Tick(Scene& scene, Gfx::Image& outputImage)
{
    // update physics
    if (isPlaying)
    {
        scene.GetPhysicsScene().Tick();

        for (auto go : scene.GetRootObjects())
        {
            TickGameObject(go);
        }
    }

    // render
    auto camera = scene.GetMainCamera();
    FrameGraph::Graph* graph = camera->GetFrameGraph();

    if (graph && graph->IsCompiled())
    {
        graph->SetScreenSize(outputImage.GetDescription().width, outputImage.GetDescription().height);
        graph->Execute(*cmd, scene);
    }
    GetGfxDriver()->ExecuteCommandBuffer(*cmd);

    cmd->Reset(true);
    return graph->GetOutputImage();
}

void GameLoop::Play()
{
    isPlaying = true;
}

void GameLoop::Stop()
{
    isPlaying = false;
}

void GameLoop::RenderScene() {}
