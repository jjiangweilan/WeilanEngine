#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;

struct Input
{
    float4 texelSize; // xy: 1/width, 1/height; zw: width, height
    float4 flags; // x: hasRTGI, y: hasGI
    float giIntensityScale;

    Texture2D rtgiTex;
    Texture2D giIrradianceTex;
    Texture2D ssaoTex;
    Texture2D albedoTex;
    [format("rgba16f")]
    RWTexture2D<float4> colorTex;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 texelSize = input.texelSize.xy;
    uint2 coord = dispatchThreadID.xy;

    float2 uv = (float2(coord) + 0.5) * texelSize;
    float3 albedo = input.albedoTex.Load(int3(coord, 0)).rgb;
    float4 currentColor = input.colorTex[coord];
    float3 newColor = currentColor.rgb;

    if (input.flags.x > 0.5)
    {
        float3 rtgi = input.rtgiTex.SampleLevel(input.sampler_linear_clamp, uv, 0).rgb;
        newColor += rtgi;
    }

    if (input.flags.y > 0.5)
    {
        float3 giIrradiance = input.giIrradianceTex.SampleLevel(input.sampler_linear_clamp, uv, 0).rgb;
        float ssao = input.ssaoTex.SampleLevel(input.sampler_linear_clamp, uv, 0).r;
        float3 diffuseGI = giIrradiance * albedo * input.giIntensityScale * ssao;
        newColor += diffuseGI;
    }

    input.colorTex[coord] = float4(newColor, currentColor.a);
}
