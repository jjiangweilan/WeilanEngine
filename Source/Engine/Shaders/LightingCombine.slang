#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;
import Lighting;

#define M_PI 3.14159265358979323846

float3 DecodeLinearSH(float4 sh0, float4 sh1, float4 sh2, float3 normal)
{
    static const float A0 = 3.141593;    // PI
    static const float A1 = 2.094395;    // 2 * PI / 3
    static const float Y00 = 0.28209479; // 1 / sqrt(4 * PI)
    static const float Y1  = 0.48860251; // sqrt(3 / (4 * PI))

    float4 bases = float4(
        A0 * Y00,
        A1 * Y1 * normal.x,
        A1 * Y1 * normal.y,
        A1 * Y1 * normal.z
    );

    float r = dot(sh0, bases);
    float g = dot(sh1, bases);
    float b = dot(sh2, bases);

    return max(float3(r, g, b), 0.0);
}

float3 SHHallucinateZH3Irradiance(float3 sh[4], float3 direction) {
    // Use the zonal axis from the luminance SH.
    const float3 lumCoeffs = float3(0.2126f, 0.7152f, 0.0722f); // sRGB luminance.
    float3 zonalAxis = normalize(float3(-dot(sh[3], lumCoeffs), -dot(sh[1], lumCoeffs), dot(sh[2], lumCoeffs)));
    float3 ratio = 0.0;
    ratio.r = abs(dot(float3(-sh[3].r, -sh[1].r, sh[2].r), zonalAxis));
    ratio.g = abs(dot(float3(-sh[3].g, -sh[1].g, sh[2].g), zonalAxis));
    ratio.b = abs(dot(float3(-sh[3].b, -sh[1].b, sh[2].b), zonalAxis));
    ratio /= sh[0];
    float3 zonalL2Coeff = sh[0] * (0.08f * ratio + 0.6f * ratio * ratio); // Curve−fit; Section 3.4.3
    float fZ = dot(zonalAxis, direction);
    float zhDir = sqrt(5.0f / (16.0f * M_PI)) * (3.0f * fZ * fZ - 1.0f);
    // Convolve sh with the normalized cosine kernel (multiply the L1 band by the zonal scale 2/3), then dot with
    // SH(direction) for linear SH (Equation 5).
    float3 result = DecodeLinearSH(float4(sh[0].r, -sh[3].r, -sh[1].r, sh[2].r), float4(sh[0].g, -sh[3].g, -sh[1].g, sh[2].g), float4(sh[0].b, -sh[3].b, -sh[1].b, sh[2].b), direction);
    // Add irradiance from the ZH3 term. zonalL2Coeff is the ZH3 coefficient for a radiance signal, so we need to
    // multiply by 1/4 (the L2 zonal scale for a normalized clamped cosine kernel) to evaluate irradiance.
    result += 0.25f * zonalL2Coeff * zhDir;
    return result;
}

struct Input
{
    float2 texelSize;
    float4 flags; // x: hasSSIL, y: hasRTGI, z: hasGISH

    Texture2D ssil;
    Texture2D rtgiTex;
    Texture2D rtgiSH0Tex;
    Texture2D rtgiSH1Tex;
    Texture2D rtgiSH2Tex;
    Texture2D normalTex;
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

    if (input.flags.z > 0.5)
    {
        float4 sh0 = input.rtgiSH0Tex.SampleLevel(input.sampler_linear_clamp, uv, 0);
        float4 sh1 = input.rtgiSH1Tex.SampleLevel(input.sampler_linear_clamp, uv, 0);
        float4 sh2 = input.rtgiSH2Tex.SampleLevel(input.sampler_linear_clamp, uv, 0);

        float3 normalWS = normalize(DecodeGBufferNormal(input.normalTex.Load(int3(coord, 0)).xyz));
        float3 sh[4] =
            {
                float3(sh0.x, sh1.x, sh2.x),
                -float3(sh0.z, sh1.z, sh2.z),
                float3(sh0.w, sh1.w, sh2.w),
                -float3(sh0.y, sh1.y, sh2.y)
            };
        //float3 irradiance = DecodeLinearSH(sh0, sh1, sh2, normalWS);
        float3 irradiance = SHHallucinateZH3Irradiance(sh, normalWS);
        float3 diffuseGI = irradiance * albedo * 3.1415926;
        newColor += diffuseGI;
    }

    input.colorTex[coord] = float4(newColor, currentColor.a);
}
