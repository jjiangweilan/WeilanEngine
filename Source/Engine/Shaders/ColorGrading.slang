#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;

struct PerMaterial
{
    Sampler2D mainColor;
};

[Material]
ParameterBlock<PerMaterial> perMaterial;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x){
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float4 color = perMaterial.mainColor.SampleLevel(input.uv, 0);
    return float4(ACESFilm(color.xyz), color.a);
}

