#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;

struct Input
{
    float2 texelSize;
    float4 flags; // x: hasSSIL, y: hasRTGI

    Texture2D ssil;
    Texture2D rtgiTex;
    Texture2D albedoTex;
    [format("rgba16f")]
    RWTexture2D<float4> colorTex;
    SamplerState sampler_linear_clamp;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 texelSize = input.texelSize;
    uint2 coord = dispatchThreadID.xy;

    float2 uv = (float2(coord) + 0.5) * texelSize;
    float3 albedo = input.albedoTex.Load(int3(coord, 0)).rgb;
    float4 currentColor = input.colorTex[coord];
    float3 newColor = currentColor.rgb;

    if (input.flags.x > 0.5)
    {
        float3 ssil = input.ssil.SampleLevel(input.sampler_linear_clamp, uv, 0).rgb;
        newColor += (ssil * albedo);
    }

    if (input.flags.y > 0.5)
    {
        float3 rtgi = input.rtgiTex.SampleLevel(input.sampler_linear_clamp, uv, 0).rgb;
        newColor += rtgi; // RTGI is already modulated by albedo and direct light in the RTGI pass
    }

    input.colorTex[coord] = float4(newColor, currentColor.a);
}
