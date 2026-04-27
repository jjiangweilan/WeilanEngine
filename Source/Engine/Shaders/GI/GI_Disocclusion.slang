import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import GI.GI_Common;

struct GIDisocclusionParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)
    float distanceScale;
    float probeDownsample;

    Texture2D<uint2> probeGeometryTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<uint2> historyProbeGeometryTex;

    RWTexture2D<float> outDisocclusionMaskTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIDisocclusionParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint2 probeCoord = MortonDecode8x8(groupIndex) + groupID.xy * 8;
    uint probeDownsample = GetGIProbeDownsample(perMaterial.probeDownsample);
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy, probeDownsample);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord;
    float2 probeUv;
    float3 probePositionWS;
    float3 probeNormalWS;
    float probePlaneThreshold;
    if (!LoadPackedProbeGeometry(
            perScene,
            perMaterial.probeGeometryTex,
            perMaterial.rtSize,
            perMaterial.distanceScale,
            probeDownsample,
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
        perMaterial.historyProbeGeometryTex,
        perMaterial.rtSize,
        probeDownsample,
        probeCoord,
        probeUv,
        probePositionWS,
        probeNormalWS,
        probePlaneThreshold,
        historyProbeCoord
    );

    perMaterial.outDisocclusionMaskTex[probeCoord] = isDisoccluded ? 1.0 : 0.0;
}
