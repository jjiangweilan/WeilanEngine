#pragma once

#define MAX_LIGHT_COUNT 128

struct Light
{
    float4 lightColor;
    float4 position;
    float ambientScale;
    float range;
    float intensity;
    float pointLightTerm1;
    float pointLightTerm2;
};

struct SphericalHarmonics_2ndOrder
{
    float4 colors[9];
};

struct Camera
{
    float4 position;
    float4 cameraZBufferParams;
    float4 cameraFrustum;// left right bottom top
    float4 screenSize;
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 invView;
    float4x4 invProjection;
    float4x4 invNDCToWorld;
    float4x4 previousViewProjection;
    float4x4 invPreviousViewProjection;
};

static const int MAX_SHADOW_MAP_CASCADE_COUNT = 4;

struct ShadowDistance
{
    float distance;
    float padding1;
    float padding2;
    float padding3;
};

struct MainLightShadow
{
    float4x4 worldToShadow[MAX_SHADOW_MAP_CASCADE_COUNT];
    float4 shadowMapSize;
    float4 cachedMainLightDirection;
    ShadowDistance shadowDistances[MAX_SHADOW_MAP_CASCADE_COUNT];
    float shadowCascadeCount;
};

struct Scene
{
    float lightCount;
    float time;
    float frameIndex;
    float _scenePad0;
    Light lights[MAX_LIGHT_COUNT];
};

struct PerScene
{
#if GPU_RESOURCE
    ConstantBuffer<Scene> scene;
    ConstantBuffer<Camera> camera;
    ConstantBuffer<MainLightShadow> mainLightShadow; 

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;

    ByteAddressBuffer globalBuffer;
    StructuredBuffer<uint> gpuObjectOffsets;
    StructuredBuffer<uint> rtObjectOffsets;
    Texture2D globalTextures[];

    ByteAddressBuffer GetGlobalBuffer() { return globalBuffer; }

    T LoadData<T>(uint byteOffset)
    {
        return globalBuffer.Load<T>(byteOffset);
    }

    T LoadData<T>(uint byteOffset, uint index)
    {
        return globalBuffer.Load<T>(byteOffset + index * sizeof(T));
    }

    Light GetMainLight()
    {
        if (scene.lightCount > 0)
        {
            return scene.lights[0];
        }
        else
            return Light(0,0,0,0,0,0,0);
    }

    float3 UvToWorldRay(float2 uv)
    {
        float4 clipPos = float4(uv * 2.0 - 1.0, camera.cameraZBufferParams.x, 1.0);
        float4 worldPos = mul(camera.invNDCToWorld, clipPos);
        worldPos /= worldPos.w;
        float3 rayDir = normalize(worldPos.xyz - camera.position.xyz);
        return rayDir;
    }

    float4 WorldToClipSpace(float4 position)
    {
        return mul(camera.viewProjection, position);
    }

    float4 WorldToClipSpace(float3 position)
    {
        return mul(camera.viewProjection, float4(position, 1.0));
    }

    float3 NDCToWorld(float3 ndcPosition)
    {
        float4 worldPos =  mul(camera.invNDCToWorld, float4(ndcPosition, 1.0));
        return worldPos.xyz / worldPos.w;
    }

    float DistanceToCamera(float3 worldPosition)
    {
        return length(worldPosition - camera.position.xyz);
    }
#endif
};

#if GPU_RESOURCE
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
    uint attributeStride;
    uint attributeFlags;
    uint padding0;
    uint padding1;

    bool HasNormal() { return (attributeFlags & 0x1) != 0; }
    bool HasTangent() { return (attributeFlags & 0x2) != 0; }
    bool HasUV() { return (attributeFlags & 0x4) != 0; }
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

struct ObjectEntity
{
    float4x4 GetModelMatrix() {return modelMatrix;}
    float4x4 GetInvModelMatrix() {return invTspModelMatrix;}
    float3 GetPosition() {return position;}
    float3 GetNormal() {return normal;}
    float4 GetTangent() {return tangent;}
    float2 GetUV() {return uv;}

    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;

    __init(ParameterBlock<PerScene> perScene, uint32_t objectOffset, uint renderDataIndex, uint vertexIndex)
    {
        GpuObject objData = perScene.LoadData<GpuObject>(objectOffset);
        GpuRenderData renderData = perScene.LoadData<GpuRenderData>(objData.pRenderDataOffset, renderDataIndex);
        GpuGeometry geometry = perScene.LoadData<GpuGeometry>(renderData.geometryOffset);

        modelMatrix = objData.model;
        invTspModelMatrix = objData.invTspModel;

        uint positionOffset = geometry.positionOffset;
        uint attributeOffset = geometry.attributeOffset;

        GpuGeometryPositionData positionData = perScene.LoadData<GpuGeometryPositionData>(positionOffset, vertexIndex);
        position = positionData.GetPosition(); 

        uint vertexOffset = geometry.attributeStride * vertexIndex;
        bool hasNormal = geometry.HasNormal();
        bool hasTangent = geometry.HasTangent();
        bool hasUV = geometry.HasUV();
        if (hasNormal)
            normal = perScene.LoadData<float3>(attributeOffset + vertexOffset);
        else
            normal = float3(0,1,0);

        if (hasTangent)
            tangent = perScene.LoadData<float4>(attributeOffset + (hasNormal ? sizeof(float3) : 0) + vertexOffset);
        else
            tangent = float4(1,0,0,1);

        if (hasUV)
            uv = perScene.LoadData<float2>(attributeOffset + (hasNormal ? sizeof(float3) : 0) + (hasTangent ? sizeof(float4) : 0) + vertexOffset);
        else
            uv = float2(0,0);
    }

};
#endif
