#pragma once

namespace Engine
{
namespace Gfx
{
class Image;
}
class Scene;
class GameLoop
{
public:
    void SetActiveScene(Scene* scene)
    {
        this->scene = scene;
    }
    Scene* GetActiveScene()
    {
        return scene;
    }

    Gfx::Image* Tick();

private:
    Scene* scene = nullptr;

    Gfx::Image* RenderScene();
};
} // namespace Engine
