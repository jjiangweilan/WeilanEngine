#pragma once
#include "Scene.hpp"

class SceneManager
{
public:
    SceneManager();

    Scene* GetActiveScene()
    {
        return scene;
    }
    void SetActiveScene(Scene& scene)
    {
        this->scene = &scene;
    }

private:
    Scene* scene = nullptr;
};
