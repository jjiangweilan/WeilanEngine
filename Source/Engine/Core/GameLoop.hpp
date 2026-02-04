#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/Runtime/System/UserInterface/UI.hpp"
#include <memory>

namespace Gfx
{
class Image;
}
class Scene;
class Camera;

// note: there should be only a single game loop because isPlaying is a static variable
class GameLoop
{
public:
    GameLoop();
    ~GameLoop();
    void SetScene(Scene& scene) { this->scene = &scene; }

    void Play();
    void Stop();
    static bool IsPlaying() { return isPlaying; }

    // I think we better render into outputImage (Like we render directly into a swapchain when we are in release
    // mode?), currently I just use it to pass some information about the screen (size)
    const void Tick(
        float2 screenSize,
        const Gfx::ImageIdentifier*& outGraphOutputImage,
        const Gfx::ImageIdentifier*& outGraphOutputDepthImage,
        bool offscreen
    );

    const Rendering::RenderPipeline& GetRenderPipeline() const { return *renderPipeline; }

private:
    static bool isPlaying;

    void RenderScene();
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    ObjPtr<Scene> scene = nullptr;

    std::unique_ptr<UI> ui;
    std::unique_ptr<Rendering::RenderPipeline> renderPipeline;
};
