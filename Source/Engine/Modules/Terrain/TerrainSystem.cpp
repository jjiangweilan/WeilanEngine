#include "TerrainSystem.hpp"

void TerrainSystem::SetTerrainRect(const float2& origin, const float2& size)
{
    terrainRect.origin = origin;
    terrainRect.size = size;

    ValidateTerrain();
}

void TerrainSystem::FrameUpdate(Camera& camera)
{
}

void TerrainSystem::ValidateTerrain()
{
}
