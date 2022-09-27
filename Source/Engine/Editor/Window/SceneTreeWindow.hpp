#pragma once

#include "Core/GameScene/GameScene.hpp"
#include "Core/GameScene/GameSceneManager.hpp"
#include "../EditorContext.hpp"
#include "Code/Ptr.hpp"
#include <unordered_map>

namespace Engine::Editor
{
    class SceneTreeWindow
    {
        public:
            SceneTreeWindow(RefPtr<EditorContext> editorContext);
            void Tick();
            RefPtr<GameScene> GetScene() {return gameScene;}
        private:

            struct GameObjectInfo
            {
                bool isExpanded;
            };

            RefPtr<EditorContext> editorContext;
            RefPtr<GameScene> gameScene;
            void DisplayGameObject(RefPtr<GameObject> obj, uint32_t& id);
            std::unordered_map<GameObject*, GameObjectInfo> gameObjectInfos;
    };
}
