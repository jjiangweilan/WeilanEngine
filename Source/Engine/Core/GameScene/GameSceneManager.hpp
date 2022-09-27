#pragma once
#include "GameScene.hpp"

namespace Engine
{
    class GameSceneManager
    {
        public:
            static RefPtr<GameSceneManager> Instance()
            {
                if (instance == nullptr)
                    instance = UniPtr<GameSceneManager>(new GameSceneManager);
                return instance;
            }
            RefPtr<GameScene> GetActiveGameScene();
            void SetActiveGameScene(RefPtr<GameScene> scene) 
            {
                activeGameScene = scene;
            }
            RefPtr<GameScene> CreateScene();

        private:
            static UniPtr<GameSceneManager> instance;
            GameSceneManager(){};
            RefPtr<GameScene> activeGameScene = nullptr;
    };
}
