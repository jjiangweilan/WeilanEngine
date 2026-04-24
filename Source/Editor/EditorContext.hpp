#pragma once
#include "Engine/Library/Math.hpp"
#include "Engine/Runtime/GameContext.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"

class Camera;
class GizmoManager;
class GameObject;
class EditorContext
{
public:
    void HighlightGameObject(GameObject* gameObject) { highLightedGameObject = gameObject; }
    GameObject* GetHighlightedGameObject() { return highLightedGameObject; }

    auto GetEditorCamera() { return editorCamera; }
    void SetEditorCamera(Camera* camera) { editorCamera = camera; }

    GameContext* GetGameContext() { return gameContext; }
    void SetGameContext(GameContext* context) { gameContext = context; }

    int4 GetSceneViewRect() const { return sceneViewRect; }
    void SetSceneViewRect(const int4& position) { sceneViewRect = position; }

    GizmoManager* GetGizmoManager() { return gizmoManager; }
    void SetGizmoManager(GizmoManager* manager) { gizmoManager = manager; }

    float2 GetMouseUVInSceneView()
    {
        auto sceneViewPos = GetSceneViewRect();
        auto mousePos = float2(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
        auto uv = (mousePos - float2(sceneViewPos)) / float2(sceneViewPos.z, sceneViewPos.w);
        return uv;
    }

private:
    Camera* editorCamera;
    GameContext* gameContext;
    GizmoManager* gizmoManager;
    GameObject* highLightedGameObject;

    int4 sceneViewRect;
};
