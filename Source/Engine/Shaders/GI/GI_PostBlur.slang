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
    Texture2D<float4> inSH0Tex;
    Texture2D<float4> inSH1Tex;
    Texture2D<float4> inSH2Tex;
    Texture2D<float> accumTex;

    [format("rgba16f")]
    RWTexture2D<float4> outSH0Tex;
    [format("rgba16f")]
    RWTexture2D<float4> outSH1Tex;
    [format("rgba16f")]
    RWTexture2D<float4> outSH2Tex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIPostBlurParameter> perMaterial;

float LoadAccumFrames(uint2 probeCoord)
{
    float accumRatio = perMaterial.accumTex.Load(int3(probeCoord, 0)).x;
    return DecodeProbeAccumulationFrames(accumRatio);
}

bool LoadPostBlurProbeGeometry(uint2 probeCoord, out float3 probePosition, out float3 probeNormal, out float probePlaneThreshold)
{
    float depth = perMaterial.hierarchyDepth.Load(int3(probeCoord, 0)).x;
    if (depth <= 0.0000001)
    {
        probePosition = 0;
        probeNormal = 0;
        probePlaneThreshold = 0;
        return false;
    }

    uint2 fullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);
    float2 uv = GetFullResUv(fullResCoord, perMaterial.rtSize);
    probePosition = ReconstructWorldPos(perScene, uv, depth);
    probeNormal = normalize(DecodeGBufferNormal(perMaterial.normalTex.Load(int3(probeCoord, 0)).xyz));
    probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, probePosition);
    return true;
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
    uint2 probeCoord = dispatchThreadID.xy;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    float3 centerPos;
    float3 centerNormal;
    float centerPlaneThreshold;
    if (!LoadPostBlurProbeGeometry(probeCoord, centerPos, centerNormal, centerPlaneThreshold))
    {
        perMaterial.outSH0Tex[probeCoord] = 0;
        perMaterial.outSH1Tex[probeCoord] = 0;
        perMaterial.outSH2Tex[probeCoord] = 0;
        return;
    }

    float centerAccumFrames = LoadAccumFrames(probeCoord);
    float centerAccumRatio = centerAccumFrames / max(perMaterial.blurParams.z, 1.0);
    int iterationIndex = (int)perMaterial.atrousParams.y;
    if (iterationIndex >= GetIterationBudget(centerAccumFrames))
    {
        perMaterial.outSH0Tex[probeCoord] = perMaterial.inSH0Tex.Load(int3(probeCoord, 0));
        perMaterial.outSH1Tex[probeCoord] = perMaterial.inSH1Tex.Load(int3(probeCoord, 0));
        perMaterial.outSH2Tex[probeCoord] = perMaterial.inSH2Tex.Load(int3(probeCoord, 0));
        return;
    }

    float minNormalDot = saturate(perMaterial.minNormalDot) * lerp(0.2f, 1, centerAccumRatio);
    float invNormalDotRange = 1.0 / max(1.0 - minNormalDot, 1e-4);
    float planeThresholdScale = lerp(4, 1, centerAccumRatio);
    int stepSize = max((int)perMaterial.atrousParams.x, 1);

    float centerWeight = kAtrousKernelWeights[0] * kAtrousKernelWeights[0];
    float4 sh0Sum = perMaterial.inSH0Tex.Load(int3(probeCoord, 0)) * centerWeight;
    float4 sh1Sum = perMaterial.inSH1Tex.Load(int3(probeCoord, 0)) * centerWeight;
    float4 sh2Sum = perMaterial.inSH2Tex.Load(int3(probeCoord, 0)) * centerWeight;
    float totalWeight = centerWeight;
    int2 maxCoord = int2(probeRtSize) - 1;

    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            int2 sampleCoordI = int2(probeCoord) + int2(x, y) * stepSize;
            if (sampleCoordI.x < 0 || sampleCoordI.y < 0 || sampleCoordI.x > maxCoord.x || sampleCoordI.y > maxCoord.y)
                continue;

            uint2 sampleCoord = uint2(sampleCoordI);
            bool isCenterSample = sampleCoord.x == probeCoord.x && sampleCoord.y == probeCoord.y;
            if (isCenterSample)
                continue;

            float3 samplePos;
            float3 sampleNormal;
            float samplePlaneThreshold;
            if (!LoadPostBlurProbeGeometry(sampleCoord, samplePos, sampleNormal, samplePlaneThreshold))
                continue;

            float planeThreshold = max(centerPlaneThreshold, samplePlaneThreshold) * planeThresholdScale;
            float normalDot = dot(centerNormal, sampleNormal);
            float normalWeight = saturate((normalDot - minNormalDot) * invNormalDotRange);
            float planeDistance = abs(dot(centerNormal, samplePos - centerPos));
            float planeWeight = saturate(1.0 - planeDistance / max(planeThreshold, 1e-4));

            float kernelWeight = kAtrousKernelWeights[abs(x)] * kAtrousKernelWeights[abs(y)];
            float weight = kernelWeight * normalWeight * planeWeight;
            if (weight <= 0.0)
                continue;

            sh0Sum += perMaterial.inSH0Tex.Load(int3(sampleCoord, 0)) * weight;
            sh1Sum += perMaterial.inSH1Tex.Load(int3(sampleCoord, 0)) * weight;
            sh2Sum += perMaterial.inSH2Tex.Load(int3(sampleCoord, 0)) * weight;
            totalWeight += weight;
        }
    }

    float invTotalWeight = 1.0 / totalWeight;
    perMaterial.outSH0Tex[probeCoord] = sh0Sum * invTotalWeight;
    perMaterial.outSH1Tex[probeCoord] = sh1Sum * invTotalWeight;
    perMaterial.outSH2Tex[probeCoord] = sh2Sum * invTotalWeight;
}
