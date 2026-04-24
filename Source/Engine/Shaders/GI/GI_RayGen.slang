import WeilanEngine;
import Library.PerScene;
import Library.RayTracing;
import Lighting;
import GI.GI_Common;

struct GIRayGenParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float secondary_bounce;
    float lowRayCount;
    float maxRayCount;
    float stableAccumFrames;
    float distanceScale;

    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float> historyAccumTex;
    Texture2D<float> historyDepthTex;
    Texture2D historyNormalTex;
    Texture2D<float4> noiseTex;
    StructuredBuffer<float2> haltonSeq;

    RWTexture2D<float4> outRayDataTex;
    RWTexture2D<float4> outRayMetaTex;

    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;

    RaytracingAccelerationStructure sceneBVH;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIRayGenParameter> perMaterial;

float3 GetNormalWS(float2 uv)
{
    float3 normalWS = perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xyz;
    return DecodeGBufferNormal(normalWS);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 rayCoord = dispatchThreadID.xy;
    uint2 rayRtSize = GetRayAtlasResolution(perMaterial.rtSize.xy);
    if (rayCoord.x >= rayRtSize.x || rayCoord.y >= rayRtSize.y)
        return;

    uint frameIndex = (uint)perScene.scene.frameIndex;
    uint2 fullResCoord = GetRaySourceCoord(rayCoord, frameIndex, perMaterial.rtSize.xy);
    float2 fullResUv = GetFullResUv(fullResCoord, perMaterial.rtSize);

    float depth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, fullResUv, 0).x;
    if (depth <= 0.0000001)
    {
        perMaterial.outRayDataTex[rayCoord] = float4(0, 0, 0, -1);
        perMaterial.outRayMetaTex[rayCoord] = 0;
        return;
    }

    float3 posWS = ReconstructWorldPos(perScene, fullResUv, depth);
    float3 normalWS = normalize(GetNormalWS(fullResUv));
    float probePlaneThreshold = GetAdaptivePlaneDistanceThreshold(perScene, perMaterial.distanceScale, posWS);

    float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, fullResUv, 0).xy;
    float2 prevUv = fullResUv - motion;
    float minHistoryAccum = 0.0;
    if (prevUv.x >= 0.0 && prevUv.x < 1.0 && prevUv.y >= 0.0 && prevUv.y < 1.0)
    {
        uint2 historyProbeCoord = GetSHProbeCoordFromFullResUv(prevUv, perMaterial.rtSize.xy);
        uint2 historyProbeFullResCoord = GetSHProbeSourceCoord(historyProbeCoord, perMaterial.rtSize.xy);
        float2 historyProbeUv = GetFullResUv(historyProbeFullResCoord, perMaterial.rtSize);
        float historyDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, historyProbeUv, 0).x;
        if (historyDepth > 0.0000001)
        {
            float3 historyPositionWS = ReconstructPreviousWorldPos(perScene, historyProbeUv, historyDepth);
            float3 historyNormalWS = normalize(
                DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, historyProbeUv, 0).xyz)
            );
            float historyPlaneDistance = length(historyPositionWS - posWS);
            if (historyPlaneDistance <= probePlaneThreshold && dot(normalWS, historyNormalWS) >= 0.9)
            {
                minHistoryAccum = Min4(perMaterial.historyAccumTex.GatherRed(perMaterial.sampler_linear_clamp, prevUv));
            }
        }
    }

    float2 halton = perMaterial.haltonSeq[frameIndex % 32];
    float2 blueNoise = perMaterial.noiseTex[fullResCoord % 128].xy;
    float2 randVal = frac(halton + blueNoise);

    const uint totalBounce = perMaterial.secondary_bounce > 0.5 ? 2 : 1;
    const uint rayCount = ComputeAdaptiveRayCountFromHistory(
        minHistoryAccum,
        perMaterial.lowRayCount,
        perMaterial.maxRayCount,
        perMaterial.stableAccumFrames
    );

    static const float3 kLuminanceWeights = float3(0.2126, 0.7152, 0.0722);
    float3 indirectLight = 0;
    float4 representativeBasisSum = 0;
    float representativeBasisWeightSum = 0.0;
    float3 fallbackRayDir = normalWS;

    for (uint rayIndex = 0; rayIndex < rayCount; ++rayIndex)
    {
        float3 currentThroughput = 1.0;
        float3 currentOrigin = OffsetRay(posWS, normalWS);
        float3 currentNormal = normalWS;
        float3 rayContribution = 0.0;
        float3 firstRayDir = normalWS;
        float4 firstRayBasis = GetLinearSHBasis(normalWS);

        for (uint bounce = 0; bounce < totalBounce; ++bounce)
        {
            float sinTheta = 0.0;
            float2 sampleUv = frac(randVal + float2(0.6180339887, 0.7548776662) * (rayIndex * totalBounce + bounce));
            float3 rayDir = GetSemiSphereDirection(currentNormal, sampleUv, sinTheta);

            if (bounce == 0)
            {
                firstRayDir = rayDir;
                firstRayBasis = GetLinearSHBasis(rayDir);
            }

            SurfaceHit hit = GetRayHitSurface(perMaterial.sceneBVH, perScene, currentOrigin, rayDir);
            if (hit.hit)
            {
                rayContribution += currentThroughput * (hit.emissive + hit.albedo * hit.directLight);
                currentThroughput *= hit.albedo;
                currentOrigin = OffsetRay(hit.position, hit.normal);
                currentNormal = normalize(hit.normal);
            }
            else
            {
                Light mainLight = perScene.GetMainLight();
                rayContribution += currentThroughput * Sky(currentOrigin, rayDir, mainLight.position.xyz, mainLight.lightColor.a > 0.0) * 15.0;
                break;
            }
        }

        indirectLight += rayContribution;
        fallbackRayDir = rayIndex == 0 ? firstRayDir : fallbackRayDir;

        float representativeWeight = dot(max(rayContribution, 0.0), kLuminanceWeights);
        representativeBasisSum += firstRayBasis * representativeWeight;
        representativeBasisWeightSum += representativeWeight;
    }

    float invRayCount = 1.0 / max((float)rayCount, 1.0);
    float4 representativeBasis = representativeBasisWeightSum > 1e-6
        ? representativeBasisSum / representativeBasisWeightSum
        : GetLinearSHBasis(normalize(fallbackRayDir));

    perMaterial.outRayDataTex[rayCoord] = float4(max(indirectLight * invRayCount, 0.0), (float)rayCount);
    perMaterial.outRayMetaTex[rayCoord] = representativeBasis;
}
