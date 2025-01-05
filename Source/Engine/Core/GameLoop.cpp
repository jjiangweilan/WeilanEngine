#include "GameLoop.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Libs/Profiler.hpp"
#include "Profiler/Profiler.hpp"
#include "Rendering/FrameGraph/FrameGraph.hpp"
#include "Rendering/Graphics.hpp"
#include "Scene/RenderingScene.hpp"
#include "Scene/Scene.hpp"
#include <spdlog/spdlog.h>

GameLoop::GameLoop() {}

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

    Scene* scene = this->scene;
    if (scene == nullptr)
        return;

    if (isPlaying)
    {
        ENGINE_BEGIN_PROFILE("Physics Tick")
        if (isPlaying)
        {
            // update physics
            scene->GetPhysicsScene().Tick();

            // tick game objects
            for (auto go : scene->GetRootObjects())
            {
                TickGameObject(go);
            }
        }

        ENGINE_END_PROFILE
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
        renderPipeline.Render(
            *scene,
            *scene->GetMainCamera(),
            {outputImage.GetDescription().width, outputImage.GetDescription().height}
        );
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
    Graphics::GetSingleton().ClearDraws();

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
}

void GameLoop::RenderScene() {}
