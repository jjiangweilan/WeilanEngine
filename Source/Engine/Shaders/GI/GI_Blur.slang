import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;
import GI.GI_Common;

struct GIBlurParameter
{
    float4 rtSize;     // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float distanceScale;
    float probeDownsample;
    float4 blurParams; // (minRadius, maxRadius, stableAccumFrames, legacyPlaneSigma)
    float minNormalDot;

    Texture2D<uint2> probeGeometryTex;
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
ParameterBlock<GIBlurParameter> perMaterial;

bool LoadProbeGeometry(uint2 probeCoord, out float3 probePos, out float3 probeNormal)
{
    uint probeDownsample = GetGIProbeDownsample(perMaterial.probeDownsample);
    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy, probeDownsample);
    float2 probeUv = GetFullResUv(probeFullResCoord, perMaterial.rtSize);
    float depth;
    DecodeProbeGeometry(perMaterial.probeGeometryTex.Load(int3(probeCoord, 0)), depth, probeNormal);
    if (depth <= 0.0000001)
    {
        probePos = 0;
        probeNormal = 0;
        return false;
    }

    probePos = ReconstructWorldPos(perScene, probeUv, depth);
    probeNormal = normalize(probeNormal);
    return true;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint2 probeCoord = MortonDecode8x8(groupIndex) + groupID.xy * 8;
    uint probeDownsample = GetGIProbeDownsample(perMaterial.probeDownsample);
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy, probeDownsample);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    float4 centerSH0 = perMaterial.inSH0Tex.Load(int3(probeCoord, 0));
    float4 centerSH1 = perMaterial.inSH1Tex.Load(int3(probeCoord, 0));
    float4 centerSH2 = perMaterial.inSH2Tex.Load(int3(probeCoord, 0));

    float3 centerPos;
    float3 centerNormal;
    if (!LoadProbeGeometry(probeCoord, centerPos, centerNormal))
    {
        perMaterial.outSH0Tex[probeCoord] = 0;
        perMaterial.outSH1Tex[probeCoord] = 0;
        perMaterial.outSH2Tex[probeCoord] = 0;
        return;
    }

    float accumRatio = perMaterial.accumTex.Load(int3(probeCoord, 0)).x;
    int minRadius = (int)max(perMaterial.blurParams.x, 0.0);
    int maxRadius = max((int)max(perMaterial.blurParams.y, 0.0), minRadius);
    int radius = max(minRadius, (int)(accumRatio < 0.99 ? 1 : 0) * maxRadius + 1e-5);
    // int radius = (int)floor((1 - saturate(accumRatio)) * maxRadius + 1e-5);

    if (radius <= 0)
    {
        perMaterial.outSH0Tex[probeCoord] = centerSH0;
        perMaterial.outSH1Tex[probeCoord] = centerSH1;
        perMaterial.outSH2Tex[probeCoord] = centerSH2;
        return;
    }

    float centerPlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, centerPos);
    float sigma = max(radius * 0.5, 0.5);
    float sigma2 = 2.0 * sigma * sigma;

    float4 sh0Sum = 0;
    float4 sh1Sum = 0;
    float4 sh2Sum = 0;
    float totalWeight = 0.0;
    int2 probeMaxCoord = int2(probeRtSize) - 1;

    radius = 3;
    float radiusScale = radius * (accumRatio < 0.5 ? 2 : 1);
    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            int2 sampleCoordI = int2(probeCoord) + int2(x, y) * 1.5;
            if (sampleCoordI.x < 0 || sampleCoordI.y < 0 || sampleCoordI.x > probeMaxCoord.x || sampleCoordI.y > probeMaxCoord.y)
                continue;

            uint2 sampleCoord = uint2(sampleCoordI);

            float3 samplePos;
            float3 sampleNormal;
            if (!LoadProbeGeometry(sampleCoord, samplePos, sampleNormal))
                continue;

            float2 offset = float2(x, y);
            // float spatialWeight = exp(-dot(offset, offset) / sigma2);
            float samplePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, samplePos);
            float probePlaneThreshold = max(centerPlaneThreshold, samplePlaneThreshold);
            float normalWeight = dot(centerNormal, sampleNormal) >= saturate(perMaterial.minNormalDot * lerp(0.2f, 1, accumRatio)) ? 1 : 0.0;

            float planeDistance = abs(dot(centerNormal, samplePos - centerPos));
            float planeWeight = planeDistance <= probePlaneThreshold * lerp(4, 1, accumRatio) ? 1.0 : 0.0;
            float weight = normalWeight * planeWeight;
            if (weight <= 0.0)
                continue;

            sh0Sum += perMaterial.inSH0Tex.Load(int3(sampleCoord, 0)) * weight;
            sh1Sum += perMaterial.inSH1Tex.Load(int3(sampleCoord, 0)) * weight;
            sh2Sum += perMaterial.inSH2Tex.Load(int3(sampleCoord, 0)) * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight <= 1e-6)
    {
        perMaterial.outSH0Tex[probeCoord] = centerSH0;
        perMaterial.outSH1Tex[probeCoord] = centerSH1;
        perMaterial.outSH2Tex[probeCoord] = centerSH2;
        return;
    }

    float invTotalWeight = 1.0 / totalWeight;
    perMaterial.outSH0Tex[probeCoord] = sh0Sum * invTotalWeight;
    perMaterial.outSH1Tex[probeCoord] = sh1Sum * invTotalWeight;
    perMaterial.outSH2Tex[probeCoord] = sh2Sum * invTotalWeight;
}
