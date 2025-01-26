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
};
