#pragma once

struct DepthBasedFogParams
{
    float4 fogColor;
    float  fogDensity;

#if GPU_RESOURCE
    Texture2D depthTexture;
    SamplerState LinearClampSampler;
#endif
};
