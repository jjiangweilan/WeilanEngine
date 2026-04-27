import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;
import GI.GI_Common;

struct GIParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float distanceScale;

    // Full-res G-buffer inputs
    Texture2D<uint2> probeGeometryTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float> disocclusionMaskTex;
    Texture2D<float4> rayDataTex;
    Texture2D<float4> rayMetaTex;
    Texture2D<float2> noiseTex;

    // Half-res SH history (previous frame)
    Texture2D<float4> historySH0Tex;
    Texture2D<float4> historySH1Tex;
    Texture2D<float4> historySH2Tex;
    Texture2D<float> historyAccumTex;

    // Full-res history for disocclusion detection
    Texture2D<uint2> historyProbeGeometryTex;

    // Half-res SH outputs (current frame)
    RWTexture2D<float4> outSH0Tex;
    RWTexture2D<float4> outSH1Tex;
    RWTexture2D<float4> outSH2Tex;
    RWTexture2D<float> outAccumTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIParameter> perMaterial;

int2 GetVogelRayOffset(int sampleIndex, int sampleCount, float rotation)
{
    static const float kGoldenAngle = 2.39996323;
    static const float kRadiusScale = 2.0;

    float radius = sqrt((sampleIndex + 0.5) / (float)sampleCount) * kRadiusScale;
    float angle = sampleIndex * kGoldenAngle + rotation;
    return int2(round(float2(cos(angle), sin(angle)) * radius));
}

void GatherSparseRaySH(
    uint2 probeFullResCoord,
    float3 probePositionWS,
    float3 probeNormalWS,
    float probePlaneThreshold,
    out float4 outSH0,
    out float4 outSH1,
    out float4 outSH2,
    out bool hasValidCurrentSample)
{
    float2 rayGrid = ((float2(probeFullResCoord) + 0.5) / 4.0) - 0.5;
    int2 rayCoordCenter = int2(floor(rayGrid + 0.5));

    uint2 rayRtSize = GetRayAtlasResolution(perMaterial.rtSize.xy);
    int2 rayMaxCoord = int2(rayRtSize) - 1;
    uint frameIndex = (uint)perScene.scene.frameIndex;
    float totalWeight = 0.0;
    float rotation = perMaterial.noiseTex[uint2(probeFullResCoord.x % 128, probeFullResCoord.y % 128)].x * 6.28318530718;

    outSH0 = 0;
    outSH1 = 0;
    outSH2 = 0;
    hasValidCurrentSample = false;

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        int2 sampleOffset = GetVogelRayOffset(i, 9, rotation);
        uint2 rayCoord = uint2(clamp(rayCoordCenter + sampleOffset * 2, int2(0, 0), rayMaxCoord));
        SparseRaySample sample = LoadSparseRaySample(
            perScene,
            perMaterial.probeGeometryTex,
            perMaterial.rayDataTex,
            perMaterial.rayMetaTex,
            perMaterial.rtSize,
            rayCoord,
            frameIndex
        );

        float weight = ComputeRayFeatureWeight(
            sample,
            probePositionWS,
            probeNormalWS,
            probePlaneThreshold
        );
        if (weight <= 0.0)
            continue;

        float4 shBasis = sample.shBasis;
        outSH0 += shBasis * sample.radiance.r * weight;
        outSH1 += shBasis * sample.radiance.g * weight;
        outSH2 += shBasis * sample.radiance.b * weight;
        totalWeight += weight;
    }

    if (totalWeight > 1e-6)
    {
        float invTotalWeight = 1.0 / totalWeight;
        outSH0 *= invTotalWeight;
        outSH1 *= invTotalWeight;
        outSH2 *= invTotalWeight;
        hasValidCurrentSample = true;
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint2 probeCoord = MortonDecode8x8(groupIndex) + groupID.xy * 8;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);
    float2 probeUv = GetFullResUv(probeFullResCoord, perMaterial.rtSize);
    float depth;
    float3 probeNormalWS;
    DecodeProbeGeometry(perMaterial.probeGeometryTex.Load(int3(probeCoord, 0)), depth, probeNormalWS);
    if (depth <= 0.0000001)
    {
        perMaterial.outSH0Tex[probeCoord] = 0;
        perMaterial.outSH1Tex[probeCoord] = 0;
        perMaterial.outSH2Tex[probeCoord] = 0;
        perMaterial.outAccumTex[probeCoord] = 0;
        return;
    }

    float3 probePositionWS = ReconstructWorldPos(perScene, probeUv, depth);
    probeNormalWS = normalize(probeNormalWS);
    float probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, probePositionWS);

    float4 currentSH0 = 0;
    float4 currentSH1 = 0;
    float4 currentSH2 = 0;
    bool isDisoccluded = perMaterial.disocclusionMaskTex.Load(int3(probeCoord, 0)).x > 0.5;
    bool hasValidCurrentSample = false;
    GatherSparseRaySH(
        probeFullResCoord,
        probePositionWS,
        probeNormalWS,
        probePlaneThreshold,
        currentSH0,
        currentSH1,
        currentSH2,
        hasValidCurrentSample
    );

    float2 motion = perMaterial.motionVectorTex.Load(int3(probeCoord, 0)).xy;
    float2 prevUV = probeUv - motion;

    float4 historySH0 = 0;
    float4 historySH1 = 0;
    float4 historySH2 = 0;
    float historyAccum = 0;
    float accumRatio = 0.0;
    float alpha = 1.0;

    uint2 historyCenterCoord = GetSHProbeCoordFromFullResUv(prevUV, perMaterial.rtSize.xy);
    int2 probeMaxCoord = int2(probeRtSize) - 1;
    float bestWeight = 0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            int2 candidateCoordI = int2(historyCenterCoord) + int2(x, y);
            if (candidateCoordI.x < 0 || candidateCoordI.y < 0 || candidateCoordI.x > probeMaxCoord.x || candidateCoordI.y > probeMaxCoord.y)
                continue;

            uint2 candidateCoord = uint2(candidateCoordI);
            uint2 candidateFullResCoord = GetSHProbeSourceCoord(candidateCoord, perMaterial.rtSize.xy);
            float2 candidateUv = GetFullResUv(candidateFullResCoord, perMaterial.rtSize);
            float candidateDepth;
            float3 candidateNormalWS;
            DecodeProbeGeometry(perMaterial.historyProbeGeometryTex.Load(int3(candidateCoord, 0)), candidateDepth, candidateNormalWS);
            if (candidateDepth <= 0.0000001)
                continue;

            float3 candidatePositionWS = ReconstructPreviousWorldPos(perScene, candidateUv, candidateDepth);
            candidateNormalWS = normalize(candidateNormalWS);
            float candidatePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, candidatePositionWS);
            float probeCandidatePlaneThreshold = candidatePlaneThreshold;

            float normalWeight = dot(probeNormalWS, candidateNormalWS) >= 0.85 ? 1 : 0;

            float3 cameraViewDir = normalize(candidatePositionWS - perScene.camera.position.xyz);
            float viewDotNorm = dot(cameraViewDir, candidateNormalWS);

            float planeDistance = abs(dot(probeNormalWS, candidatePositionWS - probePositionWS));
            float planeWeight = saturate(1 - planeDistance / probeCandidatePlaneThreshold);


            float distanceWeight = saturate(1.0 - length(candidatePositionWS - probePositionWS) / (probePlaneThreshold * lerp(100, 1, 1 - viewDotNorm)));

            float weight = normalWeight * planeWeight * distanceWeight;
            if (weight > bestWeight)
            {
                historySH0 = perMaterial.historySH0Tex.Load(int3(candidateCoord, 0));
                historySH1 = perMaterial.historySH1Tex.Load(int3(candidateCoord, 0));
                historySH2 = perMaterial.historySH2Tex.Load(int3(candidateCoord, 0));
                bestWeight = weight;

                float candidateAccumRatio = perMaterial.historyAccumTex.Load(int3(candidateCoord, 0)).x;
                historyAccum = candidateAccumRatio;
            }
        }
    }

    if (!hasValidCurrentSample)
    {
        accumRatio = historyAccum;
        alpha = 0.0;
    }
    else
    {
        accumRatio = min(historyAccum + EncodeProbeAccumulationFrames(1.0), 1.0);
        alpha = 1 / (DecodeProbeAccumulationFrames(accumRatio));
    }

    perMaterial.outSH0Tex[probeCoord] = lerp(historySH0, currentSH0, alpha);
    perMaterial.outSH1Tex[probeCoord] = lerp(historySH1, currentSH1, alpha);
    perMaterial.outSH2Tex[probeCoord] = lerp(historySH2, currentSH2, alpha);
    perMaterial.outAccumTex[probeCoord] = accumRatio;
}
