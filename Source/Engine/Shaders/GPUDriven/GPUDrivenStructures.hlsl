#pragma once

struct GpuGeometryPositionData
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
struct GpuMaterial
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
    uint shaderHash;
};

// geometry data stored in globalBuffer for bindless GPU-driven rendering.
struct GpuGeometry
{
    uint indexCount;
    uint indexOffset;
    uint positionOffset;
    uint attributeOffset;
};

struct GpuRenderData
{
    uint geometryOffset;
    uint materialOffset;
    uint shaderID;
    uint padding0;
};

struct GpuObject
{
    float4x4 model;
    float4x4 invTspModel;
    uint renderDataCount;
    uint pRenderDataOffset;
    uint padding0;
    uint padding1;
};
