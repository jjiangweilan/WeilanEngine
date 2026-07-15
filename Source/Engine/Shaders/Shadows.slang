#pragma once
import Library.PerScene;

#include "Shadows.hlsl"

#define SHADOW_PCF_QUALITY_LOW 1
#define SHADOW_PCF_QUALITY_MEDIUM 2
#define SHADOW_PCF_QUALITY_HIGH 3

#ifndef SHADOW_PCF_QUALITY
#define SHADOW_PCF_QUALITY SHADOW_PCF_QUALITY_HIGH
#endif

// Low: 3x3 / 4 samples, medium: 5x5 / 9 samples, high: 7x7 / 16 samples.

float GetTentBaseWeight(int texelOffset, int radius)
{
    return max(float(radius + 1 - abs(texelOffset)), 0.0f);
}

float2 GetTentTap(float2 phase, int pairStart, int radius, out float2 tapWeight)
{
    float2 firstWeight = lerp(
        GetTentBaseWeight(pairStart, radius),
        GetTentBaseWeight(pairStart - 1, radius),
        phase);
    float2 secondWeight = lerp(
        GetTentBaseWeight(pairStart + 1, radius),
        GetTentBaseWeight(pairStart, radius),
        phase);

    tapWeight = firstWeight + secondWeight;
    return (float(pairStart) * firstWeight + float(pairStart + 1) * secondWeight) / tapWeight;
}

void GetTentKernelFrame(float2 shadowCoord, float2 shadowMapTexelSize, out float2 baseUV, out float2 phase)
{
    float2 texelPosition = shadowCoord / shadowMapTexelSize - 0.5f;
    float2 baseTexel = floor(texelPosition);
    phase = frac(texelPosition);
    baseUV = (baseTexel + 0.5f) * shadowMapTexelSize;
}

float2 ClampShadowTap(float2 uv, float2 shadowMapTexelSize, float4 shadowUVBounds)
{
    float2 halfTexel = shadowMapTexelSize * 0.5f;
    return clamp(uv, shadowUVBounds.xy + halfTexel, shadowUVBounds.zw - halfTexel);
}

float PcfShadow(
    float2 shadowCoord,
    float2 shadowMapTexelSize,
    float4 shadowUVBounds,
    float objShadowDepth,
    float kernelScale,
    Texture2D shadowMap,
    SamplerComparisonState samplerState)
{
#if SHADOW_PCF_QUALITY == SHADOW_PCF_QUALITY_LOW
    const int radius = 1;
    const int tapCount = 2;
#elif SHADOW_PCF_QUALITY == SHADOW_PCF_QUALITY_MEDIUM
    const int radius = 2;
    const int tapCount = 3;
#elif SHADOW_PCF_QUALITY == SHADOW_PCF_QUALITY_HIGH
    const int radius = 3;
    const int tapCount = 4;
#else
#error SHADOW_PCF_QUALITY must be SHADOW_PCF_QUALITY_LOW, SHADOW_PCF_QUALITY_MEDIUM, or SHADOW_PCF_QUALITY_HIGH
#endif

    float2 baseUV;
    float2 phase;
    GetTentKernelFrame(shadowCoord, shadowMapTexelSize, baseUV, phase);

    float2 tapOffsets[tapCount];
    float2 tapWeights[tapCount];
    [unroll]
    for (int i = 0; i < tapCount; ++i)
    {
        tapOffsets[i] = GetTentTap(phase, -radius + i * 2, radius, tapWeights[i]);
    }

    float shadow = 0.0f;
    float weightSum = 0.0f;
    [unroll]
    for (int y = 0; y < tapCount; ++y)
    {
        [unroll]
        for (int x = 0; x < tapCount; ++x)
        {
            float2 tapUV = baseUV + float2(tapOffsets[x].x, tapOffsets[y].y) * shadowMapTexelSize;
            tapUV = shadowCoord + (tapUV - shadowCoord) * kernelScale;
            tapUV = ClampShadowTap(tapUV, shadowMapTexelSize, shadowUVBounds);

            float weight = tapWeights[x].x * tapWeights[y].y;
            shadow += shadowMap.SampleCmpLevelZero(samplerState, tapUV, objShadowDepth).x * weight;
            weightSum += weight;
        }
    }
    return shadow / weightSum;
}

float2 ClampShadowCascadeUVRange(float2 shadowUV, uint cascadeIndex, float shadowCascadeCount, float2 texelSize)
{
    float cascadeWidth = 1.0f / max(shadowCascadeCount, 1.0f);
    float minU = cascadeWidth * cascadeIndex;
    float maxU = minU + cascadeWidth;

    // Slightly shrink the range to keep sampling safely inside this cascade's tile.
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
    float cascadeCount = max(mainLightShadow.shadowCascadeCount, 1.0f);
    float cascadeWidth = 1.0f / cascadeCount;
    float2 shadowMapTexelSize = float2(mainLightShadow.shadowMapSize.z / cascadeCount, mainLightShadow.shadowMapSize.w);
    float4 shadowUVBounds = float4(cascadeWidth * cascadeIndex, 0.0f, cascadeWidth * (cascadeIndex + 1), 1.0f);
    shadowUV = ApplyShadowmapCascadeOffset(shadowUV, cascadeIndex, mainLightShadow.shadowMapSize, mainLightShadow.shadowCascadeCount);
    shadowUV = ClampShadowCascadeUVRange(shadowUV, cascadeIndex, mainLightShadow.shadowCascadeCount, shadowMapTexelSize);

    float objShadowDepth = shadowCS.z - shadowBias;
    bool valid = all(float3(shadowUV, shadowCS.z) > 0) && all(float3(shadowUV, shadowCS.z) < 1);
    if (!valid)
    {
        return 1.0f;
    }
    return PcfShadow(
        shadowUV,
        shadowMapTexelSize,
        shadowUVBounds,
        objShadowDepth,
        1.0f / float(cascadeIndex + 1),
        shadowMap,
        shadowSampler);
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
