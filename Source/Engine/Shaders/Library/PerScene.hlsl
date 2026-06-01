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
    float previousTime;
    Light lights[MAX_LIGHT_COUNT];
};

struct GpuDrawExtra
{
    uint32_t objectOffset;
    uint32_t renderDataIndex;
};

struct PerScene
{
#if GPU_RESOURCE
    ConstantBuffer<Scene> scene;
    ConstantBuffer<Camera> camera;
    ConstantBuffer<MainLightShadow> mainLightShadow; 

    // Global sampler table: index = addressMode*2 + filterMode
    // addressMode: Repeat=0, MirroredRepeat=1, ClampToEdge=2, ClampToBorder=3, MirrorClampToEdge=4(fallback)
    // filterMode:  Nearest=0, Linear=1
    SamplerState globalSamplers[10];

    ByteAddressBuffer globalBuffer;
    ByteAddressBuffer globalDynamicBuffer;
    StructuredBuffer<GpuDrawExtra> gpuObjectOffsets;
    StructuredBuffer<uint> rtObjectOffsets;
    Texture2D globalTextures[];

    ByteAddressBuffer GetGlobalBuffer() { return globalBuffer; }
    ByteAddressBuffer GetGlobalDynamicBuffer() { return globalDynamicBuffer; }

    SamplerState GetGlobalSampler(uint index)
    {
        return globalSamplers[min(index, 9u)];
    }

    T LoadData<T>(uint byteOffset)
    {
        return globalBuffer.Load<T>(byteOffset);
    }

    T LoadData<T>(uint byteOffset, uint index)
    {
        return globalBuffer.Load<T>(byteOffset + index * sizeof(T));
    }

    T LoadDynamicData<T>(uint byteOffset)
    {
        return globalDynamicBuffer.Load<T>(byteOffset);
    }

    T LoadDynamicData<T>(uint byteOffset, uint index)
    {
        return globalDynamicBuffer.Load<T>(byteOffset + index * sizeof(T));
    }
Light GetMainLight()
{
    if (scene.lightCount > 0)
    {
        return scene.lights[0];
    }
    Light l;
    l.lightColor = float4(0);
    l.intensity = 0;
    l.position = float4(0);
    l.ambientScale = 0;
    l.range = 0;
    l.pointLightTerm1 = 0;
    l.pointLightTerm2 = 0;
    return l;
}

float3 UvToWorldRay(float2 uv)    {
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
    uint2 baseColorTexIndex;
    uint2 normalMapTexIndex;
    uint2 metallicRoughnessTexIndex;
    uint2 emissiveMapTexIndex;
    uint shaderHash;
    uint _pad0;
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
    uint normalOffset;
    uint tangentOffset;
    uint uvOffset;
    uint boneOffset;

    bool HasNormal() { return (attributeFlags & 0x1) != 0; }
    bool HasTangent() { return (attributeFlags & 0x2) != 0; }
    bool HasUV() { return (attributeFlags & 0x4) != 0; }
    bool HasBone() { return (attributeFlags & 0x8) != 0; }
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
    uint skeletonOffset;
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
    float4 GetBone() {return bone;}

    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
    float4 bone;
    int skeletonOffset;

    __init(ParameterBlock<PerScene> perScene, uint32_t objectOffset, uint renderDataIndex, uint vertexIndex)
    {
        GpuObject objData = perScene.LoadData<GpuObject>(objectOffset);
        GpuRenderData renderData = perScene.LoadData<GpuRenderData>(objData.pRenderDataOffset, renderDataIndex);
        GpuGeometry geometry = perScene.LoadData<GpuGeometry>(renderData.geometryOffset);

        skeletonOffset = objData.skeletonOffset;

        modelMatrix = objData.model;
        invTspModelMatrix = objData.invTspModel;

        uint positionOffset = geometry.positionOffset;
        uint attributeOffset = geometry.attributeOffset;

        GpuGeometryPositionData positionData = perScene.LoadData<GpuGeometryPositionData>(positionOffset, vertexIndex);
        position = positionData.GetPosition(); 

        uint vertexOffset = geometry.attributeStride * vertexIndex;
        attributeOffset += vertexOffset;
        bool hasNormal = geometry.HasNormal();
        bool hasTangent = geometry.HasTangent();
        bool hasUV = geometry.HasUV();
        bool hasBone = geometry.HasBone();
        if (hasNormal)
            normal = perScene.LoadData<float3>(attributeOffset + geometry.normalOffset);
        else
            normal = float3(0,1,0);

        if (hasTangent)
            tangent = perScene.LoadData<float4>(attributeOffset + geometry.tangentOffset);
        else
            tangent = float4(1,0,0,1);

        if (hasUV)
            uv = perScene.LoadData<float2>(attributeOffset + geometry.uvOffset);
        else
            uv = float2(0,0);

        if (hasBone)
            bone = perScene.LoadData<float4>(attributeOffset + geometry.boneOffset);
        else
            bone = float4(0,0,0,0);
    }

};

struct RayHitVertexData
{
    float3 position;
    float3 normal;
    float2 uv;

    RayHitVertexData TransformToWorld(float4x4 modelMatrix, float3x3 invTspModelMatrix)
    {
        RayHitVertexData rayHitVertexData;

        rayHitVertexData.position = mul(modelMatrix, float4(this.position, 1.0)).xyz;
        rayHitVertexData.normal = normalize(mul(invTspModelMatrix, this.normal));
        rayHitVertexData.uv = this.uv;
        return rayHitVertexData;
    }
};

struct ObjectTriangle
{
    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;

    uint indexByteOffset;

    GpuObject objData;
    GpuRenderData renderData;
    GpuGeometry geometry;

    uint i[3];

    float4x4 GetModelMatrix() {return modelMatrix;}
    float4x4 GetInvModelMatrix() {return invTspModelMatrix;}

    RayHitVertexData GetRayHit(ParameterBlock<PerScene> perScene, float2 bary2)
    {
        // 1. Calculate the 3D barycentric coordinates
        float3 bary3 = float3(1.0 - bary2.x - bary2.y, bary2.x, bary2.y);

        // 6. Interpolate and return
        RayHitVertexData result;
        result.position = GetPosition(perScene, 0) * bary3.x + GetPosition(perScene, 1) * bary3.y + GetPosition(perScene, 2) * bary3.z;
        result.normal   = normalize(GetNormal(perScene, 0) * bary3.x + GetNormal(perScene, 1) * bary3.y + GetNormal(perScene, 2) * bary3.z);
        result.uv       = GetUV(perScene, 0) * bary3.x + GetUV(perScene, 1) * bary3.y + GetUV(perScene, 2) * bary3.z;

        return result;
    }

    float3 GetPosition(ParameterBlock<PerScene> perScene, uint vertexIndex)
    {
        GpuGeometryPositionData positionData = perScene.LoadData<GpuGeometryPositionData>(geometry.positionOffset + i[vertexIndex] * sizeof(GpuGeometryPositionData));
        return positionData.GetPosition(); 
    }

    float3 GetNormal(ParameterBlock<PerScene> perScene, uint vertexIndex)
    {
        if (geometry.HasNormal())
            return perScene.LoadData<float3>(geometry.attributeOffset + i[vertexIndex] * geometry.attributeStride + geometry.normalOffset);
        else
            return float3(0,0,1);
    }

    float4 GetTangent(ParameterBlock<PerScene> perScene, uint vertexIndex)
    {
        if (geometry.HasTangent())
            return perScene.LoadData<float4>(geometry.attributeOffset + i[vertexIndex] * geometry.attributeStride + geometry.tangentOffset);
        else
            return float4(1,0,0,1);
    }

    float2 GetUV(ParameterBlock<PerScene> perScene, uint vertexIndex)
    {
        if (geometry.HasUV())
            return perScene.LoadData<float2>(geometry.attributeOffset + i[vertexIndex] * geometry.attributeStride + geometry.uvOffset);
        else
            return float2(0,0);
    }

    GpuMaterial GetMaterial(ParameterBlock<PerScene> perScene)
    {
        return perScene.LoadData<GpuMaterial>(renderData.materialOffset);
    }

    __init(ParameterBlock<PerScene> perScene, uint32_t objectOffset, uint renderDataIndex, uint primitiveIndex)
    {
        objData = perScene.LoadData<GpuObject>(objectOffset);
        renderData = perScene.LoadData<GpuRenderData>(objData.pRenderDataOffset, renderDataIndex);
        geometry = perScene.LoadData<GpuGeometry>(renderData.geometryOffset);

        modelMatrix = objData.model;
        invTspModelMatrix = objData.invTspModel;

        indexByteOffset = geometry.indexOffset + (primitiveIndex * 3 * 4);
        i[0] = perScene.globalBuffer.Load<uint>(indexByteOffset);
        i[1] = perScene.globalBuffer.Load<uint>(indexByteOffset + 4);
        i[2] = perScene.globalBuffer.Load<uint>(indexByteOffset + 8);
    }

};
#endif
