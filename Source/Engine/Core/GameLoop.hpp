#pragma once

namespace Engine
{
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

    void Tick();

private:
    Scene* scene = nullptr;

    void RenderScene();
};
} // namespace Engine
