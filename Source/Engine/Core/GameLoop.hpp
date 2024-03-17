#pragma once

namespace Gfx
{
class Image;
}
class Scene;
class GameLoop
{
public:
    void Play();
    void Stop();

    void SetActiveScene(Scene* scene)
    {
        this->scene = scene;
    }
    Scene* GetActiveScene()
    {
        return scene;
    }

    void Tick();

private:
    Scene* scene = nullptr;
    bool isPlaying = false;

    void RenderScene();
};
