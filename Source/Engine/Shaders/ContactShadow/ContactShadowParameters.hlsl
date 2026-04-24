#pragma once

struct ContactShadowParameters
{
    float4 lightCoord;
    float farDepthValue;
    float nearDepthValue;
    float2 invDepthTextureSize;
    float thickness;

#if GPU_RESOURCE
    Texture2D<float> DepthTexture;

    RWTexture2D<float> OutputTexture;
    SamplerState PointBorderSampler;
#endif
};

struct ContactShadowPushConstant
{
    int2 waveOffset;
};
