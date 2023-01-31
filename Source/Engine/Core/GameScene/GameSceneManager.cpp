#include "GameSceneManager.hpp"
namespace Engine
{
RefPtr<GameScene> GameSceneManager::GetActiveGameScene() { return activeGameScene; }

UniPtr<GameSceneManager> GameSceneManager::instance = nullptr;
} // namespace Engine
