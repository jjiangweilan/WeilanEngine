#pragma once

float PcfShadow(float2 shadowCoord, float2 shadowMapSize, float objShadowDepth, Texture2D shadowMap, SamplerComparisonState samplerState)
{
    float shadow = 0;

    float x,y;

    const float halfFilterSize = 5; // 2 * halfFilterSize + 1
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
