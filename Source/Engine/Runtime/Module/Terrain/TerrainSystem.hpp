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
    glm::uvec2 layerIDTextureIndex;
    glm::uvec2 layerWeightTextureIndex;
    uint32_t layerTableOffset;
    uint32_t layerCount;
    glm::uvec2 padding;
};

struct GpuTerrainLayerData
{
    glm::uvec2 albedoRoughnessTextureIndex;
    glm::uvec2 normalTextureIndex;
    glm::uvec2 heightTextureIndex;
    glm::uvec2 metallicTextureIndex;
    glm::vec2 tileSize;
    float parallaxDepth;
    uint32_t padding;
};

static_assert(offsetof(GpuTerrainMaterialData, heightTextureIndex) == 0);
static_assert(offsetof(GpuTerrainMaterialData, normalTextureIndex) == 8);
static_assert(offsetof(GpuTerrainMaterialData, heightRangeAndSize) == 16);
static_assert(offsetof(GpuTerrainMaterialData, layerIDTextureIndex) == 32);
static_assert(offsetof(GpuTerrainMaterialData, layerWeightTextureIndex) == 40);
static_assert(offsetof(GpuTerrainMaterialData, layerTableOffset) == 48);
static_assert(offsetof(GpuTerrainMaterialData, layerCount) == 52);
static_assert(offsetof(GpuTerrainMaterialData, padding) == 56);
static_assert(sizeof(GpuTerrainMaterialData) == 64);

static_assert(offsetof(GpuTerrainLayerData, albedoRoughnessTextureIndex) == 0);
static_assert(offsetof(GpuTerrainLayerData, normalTextureIndex) == 8);
static_assert(offsetof(GpuTerrainLayerData, heightTextureIndex) == 16);
static_assert(offsetof(GpuTerrainLayerData, metallicTextureIndex) == 24);
static_assert(offsetof(GpuTerrainLayerData, tileSize) == 32);
static_assert(offsetof(GpuTerrainLayerData, parallaxDepth) == 40);
static_assert(offsetof(GpuTerrainLayerData, padding) == 44);
static_assert(sizeof(GpuTerrainLayerData) == 48);

class TerrainSystem
{
public:
    static std::unique_ptr<Mesh> CreateGridMesh(
        const float2& size,
        const float2& heightRange,
        uint32_t vertexResolution
    );
};
