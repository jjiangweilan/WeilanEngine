#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;

struct PushConstants
{
    uint tonemapMode;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct PerMaterial
{
    Sampler2D mainColor;
#if GPU_RESOURCE
    Sampler3D tonyMcMapfaceLUT;
#endif
};

[Material]
ParameterBlock<PerMaterial> perMaterial;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x){
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float3 TonyMcMapface(float3 stimulus) {
#if GPU_RESOURCE
    const float3 LUT_DIMS = float3(48.0, 48.0, 48.0);
    float3 uv = stimulus / (stimulus + 1.0);
    uv = uv * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;
    return perMaterial.tonyMcMapfaceLUT.SampleLevel(uv, 0).rgb;
#else
    return stimulus;
#endif
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float4 color = perMaterial.mainColor.SampleLevel(input.uv, 0);

    float3 tonemapped;
    if (pc.tonemapMode == 1) {
        tonemapped = TonyMcMapface(color.xyz);
    } else {
        tonemapped = ACESFilm(color.xyz);
    }

    return float4(tonemapped, color.a);
}

