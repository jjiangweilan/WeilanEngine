#pragma once
#include "Engine/Game/Input.hpp"

class UI
{
public:
    // this is in game view space
    void SetUICanvasCoordinate(float2 origin, float2 size);
    void DragOverlay();
};
