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
    float4x4 view;
    float4x4 projection;
    float4x4 viewProjection;
    float4x4 invProjection;
    float4x4 invNDCToWorld;
    float4 screenSize;
};

struct MainLightShadow
{
    float4x4 worldToShadow;
    float4 shadowMapSize;
    float4 cachedMainLightDirection;
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

    float4 ModelToClipSpace(float4 position)
    {
        return mul(camera.viewProjection, position);
    }

    float4 ModelToClipSpace(float3 position)
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
