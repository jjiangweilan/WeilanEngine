#include "GameLoop.hpp"
#include "Libs/Profiler.hpp"
#include "Profiler/Profiler.hpp"
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
    ENGINE_SCOPED_PROFILE("GameLoop - Tick");

    if (scene == nullptr || camera == nullptr)
        return;

    // update physics
    ENGINE_BEGIN_PROFILE("Physics Tick")
    if (isPlaying)
    {
        scene->GetPhysicsScene().Tick();

        for (auto go : scene->GetRootObjects())
        {
            TickGameObject(go);
        }
    }
    scene->GetPhysicsScene().DebugDraw();
    ENGINE_END_PROFILE

    // render
    ENGINE_BEGIN_PROFILE("FrameGraph")
    Rendering::FrameGraph::Graph* graph = camera ? camera->GetFrameGraph() : nullptr;

    if (graph && graph->IsCompiled())
    {
        graph->SetScreenSize(outputImage.GetDescription().width, outputImage.GetDescription().height);
        graph->Execute(*cmd, *scene, *camera);
    }

    ENGINE_END_PROFILE

    ENGINE_BEGIN_PROFILE("call GfxDriver-ExecuteCommandBuffer")
    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    ENGINE_END_PROFILE

    ENGINE_BEGIN_PROFILE("GameLoop Tick clean-up")
    cmd->Reset(true);
    Graphics::GetSingleton().ClearDraws();
    if (graph)
    {
        outGraphOutputImage = graph->GetOutputImage();
        outGraphOutputDepthImage = graph->GetOutputDepthImage();
    }
    else
        return;
    ENGINE_END_PROFILE
}

void GameLoop::Play()
{
    if (scene == nullptr)
        return;

    isPlaying = true;

    auto gos = scene->GetAllGameObjects();
    for (auto go : gos)
    {
        go->Awake();
    }
}

void GameLoop::Stop()
{
    isPlaying = false;
}

void GameLoop::RenderScene() {}
