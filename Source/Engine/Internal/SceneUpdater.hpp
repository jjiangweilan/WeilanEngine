#pragma once
#include "Core/GameScene/GameScene.hpp"
namespace Engine
{
class SceneUpdater
{
public:
    GameScene* GetScene() { return scene; }
    void Tick()
    {
        if (sceneActive && scene)
        {
            for (int i = 0; i < tickFrequency; ++i)
            {
                scene->Tick();
            }
        }
    }
    void StartScene(GameScene* scene)
    {
        this->scene = scene;
        scene->OnSceneStart();
        sceneActive = true;
    }

    void EndScene()
    {
        if (scene)
        {
            scene->OnSceneEnd();
        }
    }
    void SetTickFrequency(int i) { tickFrequency = i; }

private:
    int tickFrequency = 1;

    bool sceneActive = false;
    GameScene* scene = nullptr;
};
} // namespace Engine
