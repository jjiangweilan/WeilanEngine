#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;

struct LightingCombineParameter
{
    float2 texelSize;
    float2 _pad;
};

struct Input
{
    ConstantBuffer<LightingCombineParameter> params;
    Texture2D ssil;
    Texture2D albedoTex;
    [format("rgba16f")]
    RWTexture2D<float4> colorTex;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 texelSize = input.params.texelSize;
    uint2 coord = dispatchThreadID.xy;

    float3 ssil = input.ssil.Load(int3(coord, 0)).rgb;
    float3 albedo = input.albedoTex.Load(int3(coord, 0)).rgb;
    
    float4 currentColor = input.colorTex[coord];
    
    // Add SSIL contribution (ssil * albedo)
    float3 newColor = currentColor.rgb + (ssil * albedo);
    
    input.colorTex[coord] = float4(newColor, currentColor.a);
}
