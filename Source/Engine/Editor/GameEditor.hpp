#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "Libs/Ptr.hpp"

namespace Engine::Editor
{
class GameEditor
{
public:
    GameEditor(RefPtr<AssetDatabase> assetDatabase, RefPtr<GameSceneManager> gameSceneManager);

    void Tick();
};
} // namespace Engine::Editor
