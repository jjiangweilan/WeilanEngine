#pragma once
import Library.PerScene;

#include "Shadows.hlsl"

float PcfShadow(float2 shadowCoord, float2 shadowMapSize, float objShadowDepth, float kernelScale, Texture2D shadowMap, SamplerComparisonState samplerState)
{
    float shadow = 0;

    float x,y;

    const float halfFilterSize = max(2 * kernelScale, 0.5); // 2 * halfFilterSize + 1
    for (y = -halfFilterSize; y <= halfFilterSize; y += 1)
        for (x = -halfFilterSize; x <= halfFilterSize; x += 1)
        {
            float2 uv = shadowCoord + float2(x, y) * shadowMapSize;
            shadow += shadowMap.SampleCmpLevelZero(samplerState, uv, objShadowDepth).x;
        }

    shadow *= 1.0f / ((halfFilterSize * 2 + 1) * (halfFilterSize * 2 + 1));
    // shadow = shadowMap.SampleCmpLevelZero(samplerState, shadowCoord, objShadowDepth).x;
    return shadow;
}

float2 ClampShadowCascadeUVRange(float2 shadowUV, uint cascadeIndex, float shadowCascadeCount, float2 texelSize)
{
    float cascadeWidth = 1.0f / max(shadowCascadeCount, 1.0f);
    float minU = cascadeWidth * cascadeIndex;
    float maxU = minU + cascadeWidth;

    // Slightly shrink the range to keep sampling safely inside this cascade's tile.
    const float epsilon = 1e-4f;
    minU += texelSize.x;
    maxU -= texelSize.x;

    shadowUV.x = clamp(shadowUV.x, minU, maxU);
    return shadowUV;
}

float2 ApplyShadowmapCascadeOffset(float2 shadowUV, uint cascadeIndex, float4 shadowMapSize, float shadowCascadeCount)
{
    shadowUV.x = shadowUV.x / shadowCascadeCount;
    shadowUV.x += cascadeIndex * (1.0f / shadowCascadeCount);
    // TODO: implement atlas/array offset for cascades. Currently passthrough.
    return shadowUV;
}

float SampleCascadeShadow(
    uint cascadeIndex,
    float3 objPos,
    float3 objNormal,
    float shadowBias,
    float shadowNormalBias,
    MainLightShadow mainLightShadow,
    Texture2D shadowMap,
    SamplerComparisonState shadowSampler)
{
    float3 shadowWorldPos = objPos;
    float4 shadowCS = mul(mainLightShadow.worldToShadow[cascadeIndex], float4(shadowWorldPos + shadowNormalBias * objNormal, 1.0));
    shadowCS /= shadowCS.w;
    float2 shadowUV = shadowCS.xy * 0.5 + 0.5;
    shadowUV = ApplyShadowmapCascadeOffset(shadowUV, cascadeIndex, mainLightShadow.shadowMapSize, mainLightShadow.shadowCascadeCount);
    shadowUV = ClampShadowCascadeUVRange(shadowUV, cascadeIndex, mainLightShadow.shadowCascadeCount, mainLightShadow.shadowMapSize.zw);

    float objShadowDepth = shadowCS.z - shadowBias;
    bool valid = all(float3(shadowUV, shadowCS.z) > 0) && all(float3(shadowUV, shadowCS.z) < 1);
    if (!valid)
    {
        return 1.0f;
    }
    return PcfShadow(shadowUV, mainLightShadow.shadowMapSize.zw, objShadowDepth, 1 / (cascadeIndex + 1), shadowMap, shadowSampler);
}

float GetShadowAttenuation(float3 objPos, float3 objNormal, float shadowBias, float shadowNormalBias, 
        Camera camera,
        MainLightShadow mainLightShadow,
        Texture2D shadowMap, SamplerComparisonState shadowSampler)
{
    float cascadeCount = mainLightShadow.shadowCascadeCount;
    if (cascadeCount <= 0.0f)
    {
        return 1.0f;
    }

    int cascadeCountInt = int(cascadeCount);
    cascadeCountInt = min(max(cascadeCountInt, 1), MAX_SHADOW_MAP_CASCADE_COUNT);

    float distanceToCamera = length(objPos - camera.position.xyz);

    int primaryCascade = cascadeCountInt - 1;
    for (int i = 0; i < cascadeCountInt; ++i)
    {
        if (distanceToCamera <= mainLightShadow.shadowDistances[i].distance)
        {
            primaryCascade = i;
            break;
        }
    }

    float primaryShadow = SampleCascadeShadow(primaryCascade, objPos, objNormal, shadowBias, shadowNormalBias, mainLightShadow, shadowMap, shadowSampler);

    // Smoothly blend to the next cascade near the boundary.
    if (primaryCascade < cascadeCountInt - 1)
    {
        float boundaryDistance = mainLightShadow.shadowDistances[primaryCascade].distance;
        float blendRange = max(boundaryDistance * 0.05f, 0.01f);
        float t = saturate((distanceToCamera - (boundaryDistance - blendRange)) / (2.0f * blendRange));

        if (t > 0.0f)
        {
            float secondaryShadow = SampleCascadeShadow(primaryCascade + 1, objPos, objNormal, shadowBias, shadowNormalBias, mainLightShadow, shadowMap, shadowSampler);
            return lerp(primaryShadow, secondaryShadow, t);
        }
    }

    return primaryShadow;
}

float GetPointLightShadowAttenuation(
    float3 objPos,
    float3 lightPos,
    float farPlane,
    float depthBias,
    TextureCube shadowCubemap,
    SamplerComparisonState shadowSampler)
{
    float3 fragToLight = objPos - lightPos;
    // Standard linear depth: stored = d_occ/far, ref = d_frag/far - bias
    // GREATER_OR_EQUAL: stored >= ref → lit (occluder at or beyond fragment)
    float dist = length(fragToLight);
    float ref = (dist / farPlane - depthBias);
    ref = 1  - clamp(ref, 0.0f, 1.0f);
    float result = shadowCubemap.SampleCmpLevelZero(shadowSampler, normalize(fragToLight), ref);
    return result;
}
