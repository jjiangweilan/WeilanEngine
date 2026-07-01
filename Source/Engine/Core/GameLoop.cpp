#include "GameLoop.hpp"
#include "Engine/Core/Profiler/Profiler.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/MiddleLayer/DebugOptions.hpp"
#include "Engine/Runtime/System/SceneManager/BVHScene.hpp"
#include "Engine/Runtime/System/SceneManager/RenderingScene.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include <spdlog/spdlog.h>

GameLoop::GameLoop()
{
    renderPipeline = std::make_unique<Rendering::RenderPipeline>();
}

GameLoop::~GameLoop() {}

void GameLoop::SetScene(Scene& scene)
{
    if (this->scene.Get() == &scene)
        return;

    if (this->scene != nullptr)
        DestroyScene(*this->scene);

    this->scene = &scene;

    if (isPlaying)
        StartScene(scene);
}

void GameLoop::StartScene(Scene& scene)
{
    std::vector<GameObject*> awakedGos{};
    scene.ForEachGameObject([&awakedGos](GameObject* go)
                            {
        if (go->IsActiveInScene())
        {
            go->OnAwake();
            awakedGos.push_back(go);
        } });

    for (auto go : awakedGos)
    {
        go->OnStart();
    }
}

void GameLoop::DestroyScene(Scene& scene)
{
    scene.ForEachGameObject([](GameObject* go)
                            { go->OnDestroy(); });
}

static void TickGameObjectDebugDraw(const std::vector<ObjPtr<GameObject>>& rootObjects)
{
    static std::function<void(GameObject*)> f = [](GameObject* go)
    {
        if (go->IsEnabled())
        {
            go->DebugDraw();

            for (auto child : go->GetChildren())
            {
                f(child);
            }
        }
    };

    for (auto go : rootObjects)
    {
        if (go)
            f(go);
    }
}

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
    if (go == nullptr)
        return;

    go->IdleTick();

    for (auto child : go->GetChildren())
    {
        IdleTickGameObject(child);
    }
}

const void GameLoop::Tick(
    float2 screenSize,
    const Gfx::ImageIdentifier*& outGraphOutputImage,
    const Gfx::ImageIdentifier*& outGraphOutputDepthImage,
    bool offscreen
)
{
    auto& ui = UI::Instance();

    ui.SetUICanvasCoordinate({0, 0}, {static_cast<int>(screenSize.x), static_cast<int>(screenSize.y)});
    ui.Update();
    ui.DragOverlay();

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
            if (go)
            {
                TickGameObject(go);
            }
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
        ENGINE_BEGIN_PROFILE("GameLoop - GameObject Debug Draw");
        if (GetDebugOptions().drawGameObjectDebugDraw)
            TickGameObjectDebugDraw(rootObjects);
        ENGINE_END_PROFILE

        ENGINE_BEGIN_PROFILE("GameLoop - Physics Debug Draw");
        scene->GetPhysicsScene().DebugDraw();
        ENGINE_END_PROFILE

        ENGINE_BEGIN_PROFILE("GameLoop - Rendering Scene Tick");
        scene->GetRenderingScene().Tick();
        ENGINE_END_PROFILE

        ENGINE_BEGIN_PROFILE("GameLoop - BVH Scene Tick");
        scene->GetBVHScene().Tick();
        ENGINE_END_PROFILE

        if (!offscreen)
        {
            ENGINE_BEGIN_PROFILE("GameLoop - Render Pipeline Render");
            renderPipeline->Render(*scene, *scene->GetMainCamera(), screenSize);
            outGraphOutputImage = &renderPipeline->GetOutputColor();
            outGraphOutputDepthImage = &renderPipeline->GetOutputDepth();
            ui.RenderElements(outGraphOutputImage);

            ENGINE_END_PROFILE
        }
        else
        {
            outGraphOutputImage = nullptr;
            outGraphOutputDepthImage = nullptr;
        }
    }
    else
    {
        outGraphOutputImage = nullptr;
        outGraphOutputDepthImage = nullptr;
    }

    ENGINE_BEGIN_PROFILE("GameLoop Tick clean-up")

    ENGINE_END_PROFILE

    // GetGfxDriver()->FlushPendingCommands();
}

void GameLoop::Play()
{
    if (scene == nullptr)
        return;

    GetGfxDriver()->WaitForIdle(); // wait for gpu to idle, this ensures everying dispatched for editor is finished on GPU

    isPlaying = true;

    StartScene(*scene);

    // recreate render pipeline when playing
    renderPipeline = std::make_unique<Rendering::RenderPipeline>();

    Time::Reset();
}

void GameLoop::Stop()
{
    isPlaying = false;

    // recreate render pipeline after playing for editor
    renderPipeline = std::make_unique<Rendering::RenderPipeline>();
}

void GameLoop::RenderScene() {}

bool GameLoop::isPlaying = false;
