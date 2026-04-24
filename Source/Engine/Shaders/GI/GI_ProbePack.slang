import WeilanEngine;
import GI.GI_Common;

struct GIProbePackParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)

    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float> historyDepthTex;
    Texture2D historyNormalTex;

    [format("r32f")]
    RWTexture2D<float> outProbeDepthTex;
    [format("rgb10_a2")]
    RWTexture2D<float4> outProbeNormalTex;
    [format("rg16f")]
    RWTexture2D<float2> outProbeMotionTex;
    [format("r32f")]
    RWTexture2D<float> outHistoryProbeDepthTex;
    [format("rgb10_a2")]
    RWTexture2D<float4> outHistoryProbeNormalTex;

    SamplerState sampler_point_clamp;
};

[Material]
ParameterBlock<GIProbePackParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 probeCoord = dispatchThreadID.xy;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);

    float depth = perMaterial.hierarchyDepth.Load(int3(probeFullResCoord, 0)).x;
    float4 normal = perMaterial.normalTex.Load(int3(probeFullResCoord, 0));
    float2 motion = perMaterial.motionVectorTex.Load(int3(probeFullResCoord, 0)).xy;

    float historyDepth = perMaterial.historyDepthTex.Load(int3(probeFullResCoord, 0)).x;
    float4 historyNormal = perMaterial.historyNormalTex.Load(int3(probeFullResCoord, 0));

    perMaterial.outProbeDepthTex[probeCoord] = depth;
    perMaterial.outProbeNormalTex[probeCoord] = normal;
    perMaterial.outProbeMotionTex[probeCoord] = motion;
    perMaterial.outHistoryProbeDepthTex[probeCoord] = historyDepth;
    perMaterial.outHistoryProbeNormalTex[probeCoord] = historyNormal;
}
