#include "SceneManager.hpp"

SceneManager& SceneManager::GetSceneManager()
{
    static SceneManager m;
    return m;
}
SceneManager::SceneManager() {}
