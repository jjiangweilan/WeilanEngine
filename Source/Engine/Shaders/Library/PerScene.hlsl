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
    Light lights[MAX_LIGHT_COUNT];
};

struct PerScene
{
#if GPU_RESOURCE
    ConstantBuffer<Scene> scene;
    ConstantBuffer<Camera> camera;
    ConstantBuffer<MainLightShadow> mainLightShadow;
    
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
