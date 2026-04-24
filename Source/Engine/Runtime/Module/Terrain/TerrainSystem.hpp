#pragma once
#include "Engine/Library/Math.hpp"
#include <memory>

class Camera;

class TerrainSystem
{
public:
    void SetTerrainRect(const float2& origin, const float2& size);
    void FrameUpdate(Camera& camera);

private:
    struct
    {
        float2 origin;
        float2 size;
    } terrainRect;
    
    void ValidateTerrain();
};
