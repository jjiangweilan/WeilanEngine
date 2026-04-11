import WeilanEngine;
import Library.PerScene;
import Lighting;
import GI.GI_Common;

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

struct GIResolveParameter
{
    float4 texelSize; // xy: 1/width, 1/height; zw: width, height

    Texture2D<float4> rtgiSH0Tex;
    Texture2D<float4> rtgiSH1Tex;
    Texture2D<float4> rtgiSH2Tex;
    Texture2D normalTex;
    Texture2D hierarchyDepth;

    [format("rgba16f")]
    RWTexture2D<float4> outIrradianceTex;

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIResolveParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    float2 texelSize = perMaterial.texelSize.xy;

    if (coord.x >= (uint)perMaterial.texelSize.z || coord.y >= (uint)perMaterial.texelSize.w)
        return;

    float2 uv = (float2(coord) + 0.5) * texelSize;
    float3 normalWS = normalize(DecodeGBufferNormal(perMaterial.normalTex.Load(int3(coord, 0)).xyz));

    // Reconstruct current pixel world position from hierarchy depth mip 0
    float currentDepth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;
    float4 currentPosH = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, currentDepth, 1.0));
    float3 currentPos = currentPosH.xyz / currentPosH.w;

    static int2 probeTileSearchOffset[5] =
    {
        int2(0, 0),
        int2(1, 0),
        int2(-1, 0),
        int2(0, 1),
        int2(0, -1)
    };

    // Determine the 4 nearest SH probe coordinates (SH textures are quarter-resolution)
    uint2 shSize = GetSHProbeResolution(perMaterial.texelSize.zw);
    float2 shUvPixels = GI_FromFullResUv_to_SHProbeUv(uv, perMaterial.texelSize.zw, 1.0f / shSize.xy) * shSize;
    int2 shCoord00 = int2(floor(shUvPixels));
    int2 shMaxCoord = int2(shSize) - 1;

    float weights[4];
    float4 sh0Samples[4];
    float4 sh1Samples[4];
    float4 sh2Samples[4];
    float totalWeight = 0;

    // Search for valid probes with non-zero weight, expanding outward in a cross pattern
    for(int probeTileSearchIndex = 0; probeTileSearchIndex < 5; probeTileSearchIndex++)
    {
        int2 offsets[4] =
            {
                int2(0,0),
                int2(1,0),
                int2(0,1),
                int2(1,1)
            };

        for (int i = 0; i < 4; i++)
        {
            int2 sc = clamp(shCoord00 + probeTileSearchOffset[probeTileSearchIndex] + offsets[i], int2(0, 0), shMaxCoord);

            sh0Samples[i] = perMaterial.rtgiSH0Tex.Load(int3(sc, 0));
            sh1Samples[i] = perMaterial.rtgiSH1Tex.Load(int3(sc, 0));
            sh2Samples[i] = perMaterial.rtgiSH2Tex.Load(int3(sc, 0));

            // Reconstruct probe world position from hierarchy depth mip 1
            float2 probeUv = (float2(sc) + 0.5) / shSize;
            float probeDepth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, probeUv, 1).x;
            float4 probePosH = mul(perScene.camera.invNDCToWorld, float4(probeUv * 2.0 - 1.0, probeDepth, 1.0));
            float3 probePos = probePosH.xyz / probePosH.w;

            float3 diff = probePos - currentPos;
            float distSq = dot(diff, diff);
            float planeDist = dot(normalWS, diff);
            float w = (distSq > 0.0064 ? 0 : 1) * (planeDist > -0.1 ? 1.0 : 0.0);
            weights[i] = w;
        }

        totalWeight = weights[0] + weights[1] + weights[2] + weights[3];
        if (totalWeight >= 1e-6)
        {
            break;
        }
    }

    if (totalWeight < 1e-6)
    {
        weights[0] = 0.25; weights[1] = 0.25; weights[2] = 0.25; weights[3] = 0.25;
        totalWeight = 1.0;
    }

    float invTotal = 1.0 / totalWeight;
    float4 sh0 = (sh0Samples[0] * weights[0] + sh0Samples[1] * weights[1] + sh0Samples[2] * weights[2] + sh0Samples[3] * weights[3]) * invTotal;
    float4 sh1 = (sh1Samples[0] * weights[0] + sh1Samples[1] * weights[1] + sh1Samples[2] * weights[2] + sh1Samples[3] * weights[3]) * invTotal;
    float4 sh2 = (sh2Samples[0] * weights[0] + sh2Samples[1] * weights[1] + sh2Samples[2] * weights[2] + sh2Samples[3] * weights[3]) * invTotal;

    float3 irradiance = DecodeLinearSH(sh0, sh1, sh2, normalWS);

    // Output pure irradiance — albedo is applied later in LightingCombine
    perMaterial.outIrradianceTex[coord] = float4(irradiance, 1.0);
}
