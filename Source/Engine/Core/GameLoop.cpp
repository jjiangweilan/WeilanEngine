#include "GameLoop.hpp"
#include "Core/Time.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Profiler.hpp"
#include "Profiler/Profiler.hpp"
#include "Rendering/Graphics.hpp"
#include "Scene/RenderingScene.hpp"
#include "Scene/Scene.hpp"
#include <spdlog/spdlog.h>

GameLoop::GameLoop() {}

GameLoop::~GameLoop() {}

static void TickGameObject(GameObject* go)
{
    if (go->IsEnabled())
    {
        go->Tick();

        for (auto chil : go->GetChildren())
        {
            TickGameObject(chil);
        }
    }
}

static void IdleTickGameObject(GameObject* go)
{
    go->IdleTick();

    for (auto chil : go->GetChildren())
    {
        IdleTickGameObject(chil);
    }
}

const void GameLoop::Tick(
    float2 screenSize,
    const Gfx::RG::ImageIdentifier*& outGraphOutputImage,
    const Gfx::RG::ImageIdentifier*& outGraphOutputDepthImage
)
{
    ENGINE_SCOPED_PROFILE("GameLoop - Tick");

    Scene* scene = this->scene;
    if (scene == nullptr)
        return;

    auto rootObjects = scene->GetRootObjects();
    if (isPlaying)
    {
        ENGINE_BEGIN_PROFILE("Physics Tick")
        // update physics
        scene->GetPhysicsScene().Tick();

        // tick game objects
        for (auto go : rootObjects)
        {
            TickGameObject(go);
        }

        ENGINE_END_PROFILE
    }
    else
    {
        for (auto go : scene->GetRootObjects())
        {
            IdleTickGameObject(go);
        }
    }

    // render

    if (scene && scene->GetMainCamera())
    {
        ENGINE_BEGIN_PROFILE("GameLoop - Physics Debug Draw");
        scene->GetPhysicsScene().DebugDraw();
        ENGINE_END_PROFILE

        ENGINE_BEGIN_PROFILE("GameLoop - Rendering Scene Tick");
        scene->GetRenderingScene().Tick();
        ENGINE_END_PROFILE

        ENGINE_BEGIN_PROFILE("GameLoop - Render Pipeline Render");
        renderPipeline.Render(*scene, *scene->GetMainCamera(), screenSize);
        ENGINE_END_PROFILE

        outGraphOutputImage = &renderPipeline.GetOutputColor();
        outGraphOutputDepthImage = &renderPipeline.GetOutputDepth();
    }
    else
    {
        outGraphOutputImage = nullptr;
        outGraphOutputDepthImage = nullptr;
    }

    ENGINE_BEGIN_PROFILE("GameLoop Tick clean-up")

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
        go->OnStart();
    }
}

void GameLoop::Stop()
{
    isPlaying = false;
    auto gos = scene->GetAllGameObjects();
    for (auto go : gos)
    {
        go->OnStop();
    }
}

void GameLoop::RenderScene() {}
