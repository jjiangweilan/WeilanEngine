#pragma once

struct DeferredPBRShadingInput
{
    float4 shadowMapTexelSize;
    float shadowConstantBias;
    float shadowNormalBias;
    int   pointLightShadowLightIndex;
    float pointLightShadowFarPlane;
    // xyz = light world position, w = depth bias
    float4 pointLightShadowLightPosAndBias;
};
