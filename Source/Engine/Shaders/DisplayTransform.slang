#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;
import ColorSpace;

struct DisplayTransformInput
{
    uint4 flags;
    float4 hsv;
    float4 adjustments;
};

struct PerMaterial
{
    ConstantBuffer<DisplayTransformInput> settings;
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

float3 RGBToHSV(float3 c)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    const float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

float3 HSVToRGB(float3 c)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float3 ApplyHueValueSaturation(float3 color)
{
    if (perMaterial.settings.flags.y == 0)
    {
        return color;
    }

    float3 hsv = RGBToHSV(color);
    hsv.x = frac(hsv.x + perMaterial.settings.hsv.x);
    hsv.y = saturate(hsv.y * perMaterial.settings.hsv.y);
    hsv.z = saturate(hsv.z * perMaterial.settings.hsv.z);
    return HSVToRGB(hsv);
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float4 color = perMaterial.mainColor.SampleLevel(input.uv, 0);
    color.rgb *= exp2(perMaterial.settings.adjustments.x);

    float3 tonemapped;
    if (perMaterial.settings.flags.x == 1) {
        tonemapped = TonyMcMapface(color.xyz);
    } else if (perMaterial.settings.flags.x == 0) {
        tonemapped = ACESFilm(color.xyz);
    } else {
        tonemapped = saturate(color.xyz);
    }

    tonemapped = ApplyHueValueSaturation(tonemapped);

    let displayColor = pow(tonemapped, perMaterial.settings.adjustments.y);

    return float4(displayColor, color.a);
}
