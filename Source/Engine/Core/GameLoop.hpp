#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include <memory>

namespace Gfx
{
class Image;
}
class Scene;
class GameLoop
{
public:
    GameLoop();
    ~GameLoop();
    void SetScene(Scene& scene)
    {
        this->scene = &scene;
    }

    void Play();
    void Stop();

    // I think we better render into outputImage (Like we render directly into a swapchain when we are in release
    // mode?), currently I just use it to pass some information about the screen (size)
    const void Tick(
        Gfx::Image& outputImage,
        const Gfx::RG::ImageIdentifier*& outGraphOutputImage,
        const Gfx::RG::ImageIdentifier*& outGraphOutputDepthImage
    );

private:
    bool isPlaying = false;

    void RenderScene();
    std::unique_ptr<Gfx::CommandBuffer> cmd;
    Scene* scene = nullptr;
};
