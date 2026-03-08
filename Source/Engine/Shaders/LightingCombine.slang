#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;

struct Input
{
    float2 texelSize;
    float2 ssilTexelSize;

    Texture2D ssil;
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
    float3 ssil = input.ssil.SampleLevel(input.sampler_linear_clamp, uv, 0).rgb;
    float3 albedo = input.albedoTex.Load(int3(coord, 0)).rgb;
    
    float4 currentColor = input.colorTex[coord];
    
    // Add SSIL contribution (ssil * albedo)
    float3 newColor = currentColor.rgb + (ssil * albedo);
    
    input.colorTex[coord] = float4(newColor, currentColor.a);
}
