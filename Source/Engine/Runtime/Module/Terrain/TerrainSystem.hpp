#pragma once

#include "Engine/Library/Math.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>

class Mesh;

struct GpuTerrainMaterialData
{
    glm::uvec2 heightTextureIndex;
    glm::uvec2 normalTextureIndex;
    glm::vec4 heightRangeAndSize;
};

static_assert(offsetof(GpuTerrainMaterialData, heightTextureIndex) == 0);
static_assert(offsetof(GpuTerrainMaterialData, normalTextureIndex) == 8);
static_assert(offsetof(GpuTerrainMaterialData, heightRangeAndSize) == 16);
static_assert(sizeof(GpuTerrainMaterialData) == 32);

class TerrainSystem
{
public:
    static std::unique_ptr<Mesh> CreateGridMesh(
        const float2& size,
        const float2& heightRange,
        uint32_t vertexResolution
    );
};
