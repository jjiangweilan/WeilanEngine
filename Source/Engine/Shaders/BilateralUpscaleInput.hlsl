#pragma once

struct Input
{
    float4 lowResTexSize; // the target size
    float4 highResTexSize; // the target size
    float kernelScale = 1.0f;
    float integerCoordSigma = 1.0f;
    float depthDiffSigma = 1.0f;

#if GPU_RESOURCE
    Texture2D lowColor;
    Texture2D lowDepth;
    Texture2D highDepth;

    RWTexture2D<float4> dst;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
#endif
};
