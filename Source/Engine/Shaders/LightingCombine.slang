#if CONFIG
name: LightingCombine
#endif

import WeilanEngine;
import Lighting;
import Library.PerScene;
import GI.GI_Common;

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
    float4 texelSize; // xy: 1/width, 1/height; zw: width, height
    float4 flags; // x: hasSSIL, y: hasRTGI, z: hasGISH

    Texture2D ssil;
    Texture2D rtgiTex;
    Texture2D rtgiSH0Tex;
    Texture2D rtgiSH1Tex;
    Texture2D rtgiSH2Tex;
    Texture2D normalTex;
    Texture2D albedoTex;
    Texture2D hierarchyDepth;
    [format("rgba16f")]
    RWTexture2D<float4> colorTex;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
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
        float3 normalWS = normalize(DecodeGBufferNormal(input.normalTex.Load(int3(coord, 0)).xyz));

        // Reconstruct current pixel world position from hierarchy depth mip 0
        float currentDepth = input.hierarchyDepth.SampleLevel(input.sampler_point_clamp, uv, 0).x;
        float4 currentPosH = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, currentDepth, 1.0));
        float3 currentPos = currentPosH.xyz / currentPosH.w;

        // Determine the 4 nearest SH probe coordinates (SH textures are half-resolution)
        uint2 shSize = GetSHProbeResolution(input.texelSize.zw);
        float2 shUvPixels = GI_FromFullResUv_to_SHProbeUv(uv, input.texelSize.zw, 1.0f / shSize.xy) * shSize;
        int2 shCoord00 = int2(floor(shUvPixels));
        float2 frac2 = shUvPixels - float2(shCoord00);

        // float2 shSize = GetSHProbeResolution(input.texelSize.zw);
        // float2 shUvPixels = GetFullResUv(dispatchThreadID.xy, input.texelSize.zwxy);
        // int2 shCoord00 = int2(floor(shUvPixels));

        int2 shMaxCoord = int2(shSize) - 1;

        uint frameIndex = (uint)perScene.scene.frameIndex;
        
        int2 offsets[4] =
            {
                int2(0,0),
                int2(1,0),
                int2(0,1),
                int2(1,1)
            };

        float weights[4];
        float4 sh0Samples[4];
        float4 sh1Samples[4];
        float4 sh2Samples[4];


        if (dispatchThreadID.x == 951 && dispatchThreadID.y == 644)
        {
            printf("Tille Offsets: (%d, %d), (%d, %d), (%d, %d), (%d, %d)\n", offsets[0].x, offsets[0].y, offsets[1].x, offsets[1].y, offsets[2].x, offsets[2].y, offsets[3].x, offsets[3].y);
        }

        for (int i = 0; i < 4; i++)
        {
            int2 sc = clamp(shCoord00 + offsets[i], int2(0, 0), shMaxCoord);

            sh0Samples[i] = input.rtgiSH0Tex.Load(int3(sc, 0));
            sh1Samples[i] = input.rtgiSH1Tex.Load(int3(sc, 0));
            sh2Samples[i] = input.rtgiSH2Tex.Load(int3(sc, 0));

            // Reconstruct probe world position from hierarchy depth mip 1
            float2 probeUv = (float2(sc) + 0.5) / shSize;
            float probeDepth = input.hierarchyDepth.SampleLevel(input.sampler_point_clamp, probeUv, 1).x;
            float4 probePosH = mul(perScene.camera.invNDCToWorld, float4(probeUv * 2.0 - 1.0, probeDepth, 1.0));
            float3 probePos = probePosH.xyz / probePosH.w;

            float3 diff = probePos - currentPos;
            float distSq = dot(diff, diff);
            float planeDist = dot(normalWS, diff);
            float w = (sqrt(distSq) > 0.08 ? 0 : 1) * (planeDist > -0.1 ? 1.0 : 0.0);
            weights[i] = w;

            if (dispatchThreadID.x == 951 && dispatchThreadID.y == 644)
            {
                printf("%f ", sqrt(distSq));
            }
        }

            if (dispatchThreadID.x == 951 && dispatchThreadID.y == 644)
            {
                printf("\n");
            }


        // if (dispatchThreadID.x == 951 && dispatchThreadID.y == 644)
        // {
        //     printf("Debug SH blend: weights = %f, %f, %f, %f\n", weights[0], weights[1], weights[2], weights[3]);
        // }

        float totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
        if (totalWeight < 1e-6)
        {
            weights[0] = 0.25; weights[1] = 0.25; weights[2] = 0.25; weights[3] = 0.25;
            totalWeight = 1.0;
        }

        float invTotal = 1.0 / totalWeight;
        float4 sh0 = (sh0Samples[0] * weights[0] + sh0Samples[1] * weights[1] + sh0Samples[2] * weights[2] + sh0Samples[3] * weights[3]) * invTotal;
        float4 sh1 = (sh1Samples[0] * weights[0] + sh1Samples[1] * weights[1] + sh1Samples[2] * weights[2] + sh1Samples[3] * weights[3]) * invTotal;
        float4 sh2 = (sh2Samples[0] * weights[0] + sh2Samples[1] * weights[1] + sh2Samples[2] * weights[2] + sh2Samples[3] * weights[3]) * invTotal;

        float3 sh[4] =
            {
                float3(sh0.x, sh1.x, sh2.x),
                -float3(sh0.z, sh1.z, sh2.z),
                float3(sh0.w, sh1.w, sh2.w),
                -float3(sh0.y, sh1.y, sh2.y)
            };
        float3 irradiance = SHHallucinateZH3Irradiance(sh, normalWS);
        float3 diffuseGI = irradiance * albedo * 3.1415926;
        newColor += diffuseGI;
    }

    input.colorTex[coord] = float4(newColor, currentColor.a);
}
