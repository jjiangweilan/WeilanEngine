#pragma once
#include "Engine/Game/Input.hpp"

class UI
{
public:
    // this is in game view space
    void SetUICanvasCoordinate(int2 origin, int2 size);
    void DragOverlay();

private:
    int2 canvasOrigin;
    int2 canvasSize;
};
