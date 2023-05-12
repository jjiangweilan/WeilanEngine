#include "GameSceneManager.hpp"
namespace Engine
{
RefPtr<GameScene> GameSceneManager::GetActiveGameScene() { return activeGameScene; }
} // namespace Engine
