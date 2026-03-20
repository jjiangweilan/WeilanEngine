#pragma once

struct SceneObjectPositionData
{
    float3 position;

    float3 GetPosition() { return position; }
};

struct SceneObjectAttributeData
{
    float3 normal;
    float4 tangent;
    float2 uv;

    float3 GetNormal() { return normal; }
    float4 GetTangent() { return tangent; }
    float2 GetUV() { return uv; }
};

// Material data stored in globalBuffer for bindless GPU-driven rendering.
// Texture fields are indices into globalTextures[].
// 0xFFFFFFFF means no texture bound.
struct GPUMaterialData
{
    float4 baseColorFactor;
    float4 emissive;
    float roughness;
    float metallic;
    float alphaCutoff;
    uint baseColorTexIndex;
    uint normalMapTexIndex;
    uint metallicRoughnessTexIndex;
    uint emissiveMapTexIndex;
    uint padding;
};

// Per-object data stored in globalBuffer for bindless GPU-driven rendering.
// invTspModel[0][3], [1][3], [2][3] encode mesh buffer offsets (index, position, attribute).
struct GPUSceneObjectData
{
    float4x4 model;
    float4x4 invTspModel;
    uint materialIndex;
    uint padding0;
    uint padding1;
    uint padding2;
};

// Base offsets for GPU-driven data arrays within globalBuffer
struct GPUDrivenConfig
{
    uint materialDataBaseOffset;
    uint sceneObjectDataBaseOffset;
    uint sceneObjectCount;
    uint materialCount;
};
