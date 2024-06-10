#include "GameLoop.hpp"
#include "Libs/Profiler.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "Rendering/Graphics.hpp"
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

const void GameLoop::Tick(
    Gfx::Image& outputImage,
    const Gfx::RG::ImageIdentifier*& outGraphOutputImage,
    const Gfx::RG::ImageIdentifier*& outGraphOutputDepthImage
)
{
    if (scene == nullptr || camera == nullptr)
        return;

    // update physics
    if (isPlaying)
    {
        scene->GetPhysicsScene().Tick();

        for (auto go : scene->GetRootObjects())
        {
            TickGameObject(go);
        }
    }
    scene->GetPhysicsScene().DebugDraw();

    // render
    Rendering::FrameGraph::Graph* graph = camera ? camera->GetFrameGraph() : nullptr;

    if (graph && graph->IsCompiled())
    {
        graph->SetScreenSize(outputImage.GetDescription().width, outputImage.GetDescription().height);
        graph->Execute(*cmd, *scene, *camera);
    }
    GetGfxDriver()->ExecuteCommandBuffer(*cmd);

    cmd->Reset(true);
    Graphics::GetSingleton().ClearDraws();
    if (graph)
    {
        outGraphOutputImage = graph->GetOutputImage();
        outGraphOutputDepthImage = graph->GetOutputDepthImage();
    }
    else
        return;
}

void GameLoop::Play()
{
    if (scene == nullptr)
        return;

    isPlaying = true;

    for (auto go : scene->GetAllGameObjects())
    {
        go->Awake();
    }
}

void GameLoop::Stop()
{
    isPlaying = false;
}

void GameLoop::RenderScene() {}
