#pragma once
struct Cloud
{
    float4 position;
    float delta;
    float lightStep;
    float phaseFactor;

    float cloudDensityScale;
    float absorption;
    float dualLobHenyeyGreenstein;
    float4 cloudDensity_remap;
    float2 edgeFade;
    float lightIntensityScale;
    float4 sdfScale;
    float4 lighting_remap;
    float4 worley_amp;

#if GPU_RESOURCE
    Texture3D cloudDensity;
    SamplerState sampler_Linear_ClampToBorder;
#endif
};
