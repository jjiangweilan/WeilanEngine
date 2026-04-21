import WeilanEngine;
import Library.PerScene;
import Lighting;
import GI.GI_Common;

// B3-spline kernel weights for the 5x5 a-trous stencil.
static const float kAtrousKernelWeights[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };

struct GIPostBlurParameter
{
    float4 rtSize;       // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float distanceScale;
    float4 blurParams;   // (stablePassCount, unstablePassCount, stableAccumFrames, legacyPlaneSigma)
    float4 atrousParams; // (stepSize, iterationIndex, unused, unused)
    float minNormalDot;

    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D<float4> inIrradianceTex;
    Texture2D<float> disocclusionMaskTex;
    Texture2D<float> accumTex;

    [format("rgba16f")]
    RWTexture2D<float4> outIrradianceTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIPostBlurParameter> perMaterial;

float3 GetNormalWS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

float LoadAccumFrames(uint2 fullResCoord)
{
    uint2 probeCoord = GetSHProbeCoordFromFullResCoord(fullResCoord, perMaterial.rtSize.xy);
    float accumRatio = perMaterial.accumTex.Load(int3(probeCoord, 0)).x;
    return DecodeProbeAccumulationFrames(accumRatio);
}

int GetIterationBudget(float accumFrames)
{
    int stablePassCount = max((int)perMaterial.blurParams.x, 0);
    int unstablePassCount = max((int)perMaterial.blurParams.y, stablePassCount);
    return accumFrames >= max(perMaterial.blurParams.z, 1.0) ? stablePassCount : unstablePassCount;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    uint2 rtSize = uint2(perMaterial.rtSize.xy);
    if (coord.x >= rtSize.x || coord.y >= rtSize.y)
        return;

    float depth = perMaterial.hierarchyDepth.Load(int3(coord, 0)).x;
    if (depth <= 0.0000001)
    {
        perMaterial.outIrradianceTex[coord] = 0;
        return;
    }

    float4 centerIrradiance = perMaterial.inIrradianceTex.Load(int3(coord, 0));
    float centerAccumFrames = LoadAccumFrames(coord);
    int iterationIndex = (int)perMaterial.atrousParams.y;
    if (iterationIndex >= GetIterationBudget(centerAccumFrames))
    {
        perMaterial.outIrradianceTex[coord] = centerIrradiance;
        return;
    }

    float2 uv = GetFullResUv(coord, perMaterial.rtSize);
    float3 centerPos = ReconstructWorldPos(perScene, uv, depth);
    float3 centerNormal = normalize(GetNormalWS(uv));
    float centerPlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, centerPos);
    float minNormalDot = saturate(perMaterial.minNormalDot);
    int stepSize = max((int)perMaterial.atrousParams.x, 1);

    float4 irradianceSum = 0;
    float totalWeight = 0.0;
    int2 maxCoord = int2(rtSize) - 1;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 sampleCoordI = clamp(int2(coord) + int2(x, y) * stepSize, int2(0, 0), maxCoord);
            uint2 sampleCoord = uint2(sampleCoordI);
            float sampleDepth = perMaterial.hierarchyDepth.Load(int3(sampleCoord, 0)).x;
            if (sampleDepth <= 0.0000001)
                continue;

            uint2 sampleProbeCoord = GetSHProbeCoordFromFullResCoord(sampleCoord, perMaterial.rtSize.xy);
            bool isCenterSample = sampleCoord.x == coord.x && sampleCoord.y == coord.y;
            // if (!isCenterSample && perMaterial.disocclusionMaskTex.Load(int3(sampleProbeCoord, 0)).x > 0.5)
            //     continue;

            float2 sampleUv = GetFullResUv(sampleCoord, perMaterial.rtSize);
            float3 samplePos = ReconstructWorldPos(perScene, sampleUv, sampleDepth);
            float3 sampleNormal = normalize(GetNormalWS(sampleUv));
            float samplePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, samplePos);
            float planeThreshold = max(centerPlaneThreshold, samplePlaneThreshold);
            float normalDot = dot(centerNormal, sampleNormal);
            float normalWeight = isCenterSample ? 1.0 : saturate((normalDot - minNormalDot) / max(1.0 - minNormalDot, 1e-4));
            float planeDistance = abs(dot(centerNormal, samplePos - centerPos));
            float planeWeight = isCenterSample ? 1.0 : saturate(1.0 - planeDistance / max(planeThreshold, 1e-4));

            float sampleAccumFrames = LoadAccumFrames(sampleCoord);
            float accumWeight = saturate(sampleAccumFrames / max(perMaterial.blurParams.z, 1.0));
            if (isCenterSample)
                accumWeight = 1.0;

            float kernelWeight = kAtrousKernelWeights[abs(x)] * kAtrousKernelWeights[abs(y)];
            float3 sampleIrradiance = max(perMaterial.inIrradianceTex.Load(int3(sampleCoord, 0)).rgb, 0.0);
            float weight = normalWeight * planeWeight;
            if (weight <= 0.0)
                continue;

            irradianceSum += float4(sampleIrradiance, 1.0) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight <= 1e-6)
    {
        perMaterial.outIrradianceTex[coord] = centerIrradiance;
        return;
    }

    float invTotalWeight = 1.0 / totalWeight;
    perMaterial.outIrradianceTex[coord] = float4(irradianceSum.rgb * invTotalWeight, centerIrradiance.a);
}
