import WeilanEngine;
import Library.PerScene;
import GI.GI_Common;

struct GIDisocclusionParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float distanceScale;

    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float> historyDepthTex;
    Texture2D historyNormalTex;

    RWTexture2D<float> outDisocclusionMaskTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIDisocclusionParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 probeCoord = dispatchThreadID.xy;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord;
    float2 probeUv;
    float3 probePositionWS;
    float3 probeNormalWS;
    float probePlaneThreshold;
    if (!LoadProbeGeometry(
            perScene,
            perMaterial.hierarchyDepth,
            perMaterial.normalTex,
            perMaterial.sampler_point_clamp,
            perMaterial.rtSize,
            perMaterial.distanceScale,
            probeCoord,
            probeFullResCoord,
            probeUv,
            probePositionWS,
            probeNormalWS,
            probePlaneThreshold
        ))
    {
        perMaterial.outDisocclusionMaskTex[probeCoord] = 1.0;
        return;
    }

    uint2 historyProbeCoord;
    bool isDisoccluded = EvaluateProbeDisocclusion(
        perScene,
        perMaterial.motionVectorTex,
        perMaterial.historyDepthTex,
        perMaterial.historyNormalTex,
        perMaterial.sampler_point_clamp,
        perMaterial.rtSize,
        probeCoord,
        probeUv,
        probePositionWS,
        probeNormalWS,
        probePlaneThreshold,
        historyProbeCoord
    );

    perMaterial.outDisocclusionMaskTex[probeCoord] = isDisoccluded ? 1.0 : 0.0;
}
