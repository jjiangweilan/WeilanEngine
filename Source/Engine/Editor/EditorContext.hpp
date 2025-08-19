#pragma once
#include "Core/GameContext.hpp"
#include "Libs/Math.hpp"

class Camera;
class EditorContext
{
public:
    auto GetEditorCamera() { return editorCamera; }
    void SetEditorCamera(Camera* camera) { editorCamera = camera; }

    GameContext* GetGameContext() { return gameContext; }
    void SetGameContext(GameContext* context) { gameContext = context; }

private:
    Camera* editorCamera;
    GameContext* gameContext;
};
