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
    Texture2D hierarchyDepth;
    Texture2D normalTex;
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
    Texture2D<float> historyDepthTex;
    Texture2D<float4> historyNormalTex;

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

float3 GetNormalWS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

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
            perMaterial.hierarchyDepth,
            perMaterial.normalTex,
            perMaterial.sampler_point_clamp,
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
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 probeCoord = dispatchThreadID.xy;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);
    float2 probeUv = GetFullResUv(probeFullResCoord, perMaterial.rtSize);
    float depth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, probeUv, 0).x;
    if (depth <= 0.0000001)
    {
        perMaterial.outSH0Tex[probeCoord] = 0;
        perMaterial.outSH1Tex[probeCoord] = 0;
        perMaterial.outSH2Tex[probeCoord] = 0;
        perMaterial.outAccumTex[probeCoord] = 0;
        return;
    }

    float3 probePositionWS = ReconstructWorldPos(perScene, probeUv, depth);
    float3 probeNormalWS = normalize(GetNormalWS(probeUv));
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

    float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, probeUv, 0).xy;
    float2 prevUV = probeUv - motion;

    float4 historySH0 = 0;
    float4 historySH1 = 0;
    float4 historySH2 = 0;
    float accumRatio = hasValidCurrentSample ? EncodeProbeAccumulationFrames(1.0) : 0.0;
    float alpha = 1.0;
    uint2 reprojectedCoord = 0;

    bool isOffScreen = prevUV.x < 0.0 || prevUV.x >= 1.0 || prevUV.y < 0.0 || prevUV.y >= 1.0;
    if (!isDisoccluded)
    {
        reprojectedCoord = GetSHProbeCoordFromFullResUv(prevUV, perMaterial.rtSize.xy);
        uint2 historyProbeFullResCoord = GetSHProbeSourceCoord(reprojectedCoord, perMaterial.rtSize.xy);
        float2 historyProbeUv = GetFullResUv(historyProbeFullResCoord, perMaterial.rtSize);
        float historyDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, historyProbeUv, 0).x;
        if (historyDepth > 0.0000001)
        {
            float3 historyPositionWS = ReconstructPreviousWorldPos(perScene, historyProbeUv, historyDepth);
            float3 historyNormal = normalize(DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, historyProbeUv, 0).xyz));
            float historyPlaneDistance = length(historyPositionWS - probePositionWS); // just use distance here
            if (historyPlaneDistance <= probePlaneThreshold && dot(probeNormalWS, historyNormal) >= 0.9)
            {
                historySH0 = perMaterial.historySH0Tex.Load(int3(reprojectedCoord, 0));
                historySH1 = perMaterial.historySH1Tex.Load(int3(reprojectedCoord, 0));
                historySH2 = perMaterial.historySH2Tex.Load(int3(reprojectedCoord, 0));

                float historyAccum = perMaterial.historyAccumTex.Load(int3(reprojectedCoord, 0)).x;
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
            }
        }
    }

    // for low accumulated tile, we blur from sh among 5x5 tiles
    // this fix for edges and small disocclusion, but if this is a large disocclusion, we still can't find proper probes in history to resolve
    // TODO: let borrow happen in Resolve?
    if (isDisoccluded)
    {
        uint2 historyCenterCoord = GetSHProbeCoordFromFullResUv(prevUV, perMaterial.rtSize.xy);
        int2 probeMaxCoord = int2(probeRtSize) - 1;
        float4 historyBlurredSH0 = 0;
        float4 historyBlurredSH1 = 0;
        float4 historyBlurredSH2 = 0;
        float historyAccumWeightedSum = 0.0;
        float historyTotalWeight = 0.0;

        for (int y = -2; y <= 2; ++y)
        {
            for (int x = -2; x <= 2; ++x)
            {
                int2 candidateCoordI = clamp(int2(historyCenterCoord) + int2(x, y), int2(0, 0), probeMaxCoord);
                uint2 candidateCoord = uint2(candidateCoordI);
                uint2 candidateFullResCoord = GetSHProbeSourceCoord(candidateCoord, perMaterial.rtSize.xy);
                float2 candidateUv = GetFullResUv(candidateFullResCoord, perMaterial.rtSize);
                float candidateDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, candidateUv, 0).x;
                if (candidateDepth <= 0.0000001)
                    continue;

                float3 candidatePositionWS = ReconstructPreviousWorldPos(perScene, candidateUv, candidateDepth);
                float3 candidateNormalWS = normalize(DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, candidateUv, 0).xyz));
                float candidatePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, candidatePositionWS);
                float probeCandidatePlaneThreshold = max(probePlaneThreshold, candidatePlaneThreshold);

                float normalWeight = dot(probeNormalWS, candidateNormalWS) >= 0.9 ? 1.0 : 0.0;
                if (normalWeight <= 0.0)
                    continue;

                float planeDistance = abs(dot(probeNormalWS, candidatePositionWS - probePositionWS));
                float planeWeight = planeDistance <= probeCandidatePlaneThreshold ? 1.0 : 0.0f;
                if (planeWeight <= 0.0)
                    continue;

                float candidateAccumRatio = perMaterial.historyAccumTex.Load(int3(candidateCoord, 0)).x;
                float accumWeight = max(DecodeProbeAccumulationFrames(candidateAccumRatio), 1.0);

                float distanceWeight = saturate(1.0 - length(candidatePositionWS - probePositionWS) / probePlaneThreshold);
                if (distanceWeight <= 0.0)
                    continue;

                float weight = normalWeight * planeWeight * accumWeight * distanceWeight;
                if (weight <= 0.0)
                    continue;

                historyBlurredSH0 += perMaterial.historySH0Tex.Load(int3(candidateCoord, 0)) * weight;
                historyBlurredSH1 += perMaterial.historySH1Tex.Load(int3(candidateCoord, 0)) * weight;
                historyBlurredSH2 += perMaterial.historySH2Tex.Load(int3(candidateCoord, 0)) * weight;
                historyAccumWeightedSum += candidateAccumRatio * weight;
                historyTotalWeight += weight;
            }
        }

        if (historyTotalWeight > 1e-6)
        {
            float invHistoryTotalWeight = 1.0 / historyTotalWeight;
            historySH0 = historyBlurredSH0 * invHistoryTotalWeight;
            historySH1 = historyBlurredSH1 * invHistoryTotalWeight;
            historySH2 = historyBlurredSH2 * invHistoryTotalWeight;
            accumRatio = max(accumRatio, historyAccumWeightedSum * invHistoryTotalWeight);
            alpha = 1 / (DecodeProbeAccumulationFrames(accumRatio));
        }
    }

    perMaterial.outSH0Tex[probeCoord] = lerp(historySH0, currentSH0, alpha);
    perMaterial.outSH1Tex[probeCoord] = lerp(historySH1, currentSH1, alpha);
    perMaterial.outSH2Tex[probeCoord] = lerp(historySH2, currentSH2, alpha);
    perMaterial.outAccumTex[probeCoord] = accumRatio;
}
