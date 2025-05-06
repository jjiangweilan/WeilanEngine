#pragma once
#include "Scene.hpp"

class SceneManager
{
public:
    static Scene* GetActiveScene() { return GetSceneManager().GetActiveSceneImpl(); }
    static void SetActiveScene(Scene* scene) { GetSceneManager().SetActiveSceneImpl(scene); }

private:
    SceneManager();
    static SceneManager& GetSceneManager();
    Scene* scene = nullptr;

    Scene* GetActiveSceneImpl() { return scene; }
    void SetActiveSceneImpl(Scene* scene) { this->scene = scene; }
};
