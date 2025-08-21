#pragma once
#include "Core/GameContext.hpp"
#include "Libs/Math.hpp"
#include "ThirdParty/imgui/imgui.h"

class Camera;
class GizmoManager;
class EditorContext
{
public:
    auto GetEditorCamera() { return editorCamera; }
    void SetEditorCamera(Camera* camera) { editorCamera = camera; }

    GameContext* GetGameContext() { return gameContext; }
    void SetGameContext(GameContext* context) { gameContext = context; }

    int4 GetSceneViewRect() const { return sceneViewRect; }
    void SetSceneViewRect(const int4& position) { sceneViewRect = position; }

    GizmoManager* GetGizmoManager() { return gizmoManager; }
    void SetGizmoManager(GizmoManager* manager) { gizmoManager = manager; }

    float2 GetUVInSceneView()
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

    int4 sceneViewRect;
};
