#pragma once
#include "GameScene.hpp"

namespace Engine
{
class GameSceneManager
{
public:
    GameSceneManager(){};
    RefPtr<GameScene> GetActiveGameScene();
    void SetActiveGameScene(RefPtr<GameScene> scene) { activeGameScene = scene; }

private:
    RefPtr<GameScene> activeGameScene = nullptr;
};
} // namespace Engine
