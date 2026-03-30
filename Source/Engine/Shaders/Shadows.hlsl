#pragma once

struct ShadowPass
{
    float cascadeIndex;
};

struct PointLightShadowPass
{
    float4x4 worldToShadow;
    float3   lightPosition;
    float    farPlane;
};
