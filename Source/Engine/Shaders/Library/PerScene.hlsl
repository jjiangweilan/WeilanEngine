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

struct PerScene
{
    float4 viewPos;
    matrix<float,4,4> view;
    matrix<float,4,4> projection;
    matrix<float,4,4> viewProjection;
    matrix<float,4,4> worldToShadow;
    matrix<float,4,4> invProjection;
    matrix<float,4,4> invNDCToWorld;
    float4 lightCount; // x: lightCount
    float4 shadowMapSize;
    float4 cameraZBufferParams;
    float4 cameraFrustum;// left right bottom top
    float4 screenSize;
    float4 cachedMainLightDirection;
    float time;
    float padding0;
    float padding1;
    float padding2;
    Light lights[MAX_LIGHT_COUNT];

    SphericalHarmonics_2ndOrder sh_2ndOrder;

#if GPU_RESOURCE
    Light GetMainLight()
    {
        if (lightCount.x > 0)
        {
            return lights[0];
        }
        else
            return Light(0,0,0,0,0,0,0);
    }

    float4 ModelToClipSpace(float4 position)
    {
        return mul(viewProjection, position);
    }

    float4 ModelToClipSpace(float3 position)
    {
        return mul(viewProjection, float4(position, 1.0));
    }
#endif
};
