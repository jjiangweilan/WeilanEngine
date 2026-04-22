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
    float distanceScale;
    float resolveClampWeightScale;

    Texture2D<float4> rtgiSH0Tex;
    Texture2D<float4> rtgiSH1Tex;
    Texture2D<float4> rtgiSH2Tex;
    Texture2D<float> accumTex;
    Texture2D<float> disocclusionMaskTex;
    Texture2D normalTex;
    Texture2D<float2> motionVectorTex;
    Texture2D hierarchyDepth;
    Texture2D<float> historyDepthTex;
    Texture2D<float4> historyNormalTex;
    Texture2D<float4> historyIrradianceTex;

    [format("rgba16f")]
    RWTexture2D<float4> outIrradianceTex;

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIResolveParameter> perMaterial;

float CalculateLuminance(float3 value)
{
    static const float3 kLuminanceWeights = float3(0.2126, 0.7152, 0.0722);
    return dot(value, kLuminanceWeights);
}

static const float kHistoryClampSlack = 1.01;

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

    float currentDepth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;
    if (currentDepth <= 0.0000001)
    {
        perMaterial.outIrradianceTex[coord] = 0;
        return;
    }

    float4 currentPosH = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, currentDepth, 1.0));
    float3 currentPos = currentPosH.xyz / currentPosH.w;

    uint2 shSize = GetSHProbeResolution(perMaterial.texelSize.zw);
    float2 probeGrid = GetSHProbeGridFromFullResCoord(coord, perMaterial.texelSize.zw);
    int2 shCoord00 = int2(floor(probeGrid));
    int2 shMaxCoord = int2(shSize) - 1;
    float2 probeFrac = frac(probeGrid);

    static const int2 kOffsets[4] =
    {
        int2(0, 0),
        int2(1, 0),
        int2(0, 1),
        int2(1, 1)
    };

    float weights[4] =
    {
        (1.0 - probeFrac.x) * (1.0 - probeFrac.y),
        probeFrac.x * (1.0 - probeFrac.y),
        (1.0 - probeFrac.x) * probeFrac.y,
        probeFrac.x * probeFrac.y
    };

    float4 sh0Samples[4];
    float4 sh1Samples[4];
    float4 sh2Samples[4];
    float totalWeight = 0;
    float4 fullResRtSize = float4(perMaterial.texelSize.zw, perMaterial.texelSize.xy);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        int2 sc = clamp(shCoord00 + kOffsets[i], int2(0, 0), shMaxCoord);
        sh0Samples[i] = perMaterial.rtgiSH0Tex.Load(int3(sc, 0));
        sh1Samples[i] = perMaterial.rtgiSH1Tex.Load(int3(sc, 0));
        sh2Samples[i] = perMaterial.rtgiSH2Tex.Load(int3(sc, 0));
        if (perMaterial.disocclusionMaskTex.Load(int3(sc, 0)).x > 0.5)
        {
            weights[i] = 0.0;
            continue;
        }

        uint2 probeSourceCoord = GetSHProbeSourceCoord(uint2(sc), perMaterial.texelSize.zw);
        float2 probeUv = GetFullResUv(probeSourceCoord, fullResRtSize);
        float probeDepth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, probeUv, 0).x;
        if (probeDepth <= 0.0000001)
        {
            weights[i] = 0.0;
            continue;
        }

        float3 probePos = ReconstructWorldPos(perScene, probeUv, probeDepth);
        float probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, probePos);
        float3 diff = probePos - currentPos;
        float distSq = dot(diff, diff);
        float planeDist = dot(normalWS, diff);
        float accumWeight = perMaterial.accumTex.Load(int3(sc, 0)).x;
        float distWeight = saturate(1.0 - distSq / 0.0064);
        float planeWeight = saturate(1.0 - abs(planeDist) / probePlaneThreshold);

        weights[i] *= distWeight * planeWeight * (accumWeight > 0.95 ? 1 : 0);
        totalWeight += weights[i];
    }

    float3 irradiance = 0.0;
    if (totalWeight < 1e-6)
    {
        int2 fallbackCoord = int2(GetSHProbeCoordFromFullResCoord(coord, perMaterial.texelSize.zw));
        float4 sh0 = perMaterial.rtgiSH0Tex.Load(int3(fallbackCoord, 0));
        float4 sh1 = perMaterial.rtgiSH1Tex.Load(int3(fallbackCoord, 0));
        float4 sh2 = perMaterial.rtgiSH2Tex.Load(int3(fallbackCoord, 0));
        irradiance = DecodeLinearSH(sh0, sh1, sh2, normalWS);
    }
    else
    {
        float invTotal = 1.0 / totalWeight;
        float4 sh0 = (sh0Samples[0] * weights[0] + sh0Samples[1] * weights[1] + sh0Samples[2] * weights[2] + sh0Samples[3] * weights[3]) * invTotal;
        float4 sh1 = (sh1Samples[0] * weights[0] + sh1Samples[1] * weights[1] + sh1Samples[2] * weights[2] + sh1Samples[3] * weights[3]) * invTotal;
        float4 sh2 = (sh2Samples[0] * weights[0] + sh2Samples[1] * weights[1] + sh2Samples[2] * weights[2] + sh2Samples[3] * weights[3]) * invTotal;

        irradiance = DecodeLinearSH(sh0, sh1, sh2, normalWS);
    }

    uint2 nearestProbeCoord = GetSHProbeCoordFromFullResCoord(coord, perMaterial.texelSize.zw);
    bool nearestProbeDisoccluded = perMaterial.disocclusionMaskTex.Load(int3(nearestProbeCoord, 0)).x > 0.5;
    if (!nearestProbeDisoccluded)
    {
        float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xy;
        float2 prevUV = uv - motion;
        bool isOffScreen = prevUV.x < 0.0 || prevUV.x >= 1.0 || prevUV.y < 0.0 || prevUV.y >= 1.0;
        if (!isOffScreen)
        {
            float historyDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).x;
            if (historyDepth > 0.00000001)
            {
                float3 historyPosition = ReconstructPreviousWorldPos(perScene, prevUV, historyDepth);
                float3 historyNormal = normalize(
                    DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).xyz)
                );
                float probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, currentPos);
                if (length(historyPosition - currentPos) <= probePlaneThreshold && dot(normalWS, historyNormal) >= 0.9)
                {
                    float3 historyIrradiance = max(
                        perMaterial.historyIrradianceTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).xyz,
                        0.0
                    );
                    float currentLum = CalculateLuminance(irradiance);
                    float historyLum = CalculateLuminance(historyIrradiance);
                    float clampWeight = saturate(
                        saturate(perMaterial.accumTex.Load(int3(nearestProbeCoord, 0)).x)
                    );
                    float maxLum = lerp(currentLum , max(historyLum, 1e-6) * kHistoryClampSlack * perMaterial.resolveClampWeightScale, clampWeight);
                    if (currentLum > maxLum && currentLum > 1e-6)
                    {
                        irradiance *= maxLum / currentLum;
                    }
                }
            }
        }
    }

    // Output pure irradiance — albedo is applied later in LightingCombine
    perMaterial.outIrradianceTex[coord] = float4(irradiance, 1.0);
}
