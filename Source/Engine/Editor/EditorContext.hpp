#pragma once
#include "Libs/Math.hpp"

class Camera;
class EditorContext
{
public:
    auto GetEditorCamera() { return editorCamera; }
    void SetEditorCamera(Camera* camera) { editorCamera = camera; }

    float2 GetGameScreenUVFromMousePosition();

private:
    Camera* editorCamera;
};
