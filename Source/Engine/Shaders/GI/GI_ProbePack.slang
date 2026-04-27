import WeilanEngine;
import Library.CommonUtils;
import GI.GI_Common;
import Lighting;

struct GIProbePackParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)

    Texture2D hierarchyDepth;
    Texture2D normalTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float> historyDepthTex;
    Texture2D historyNormalTex;

    [format("rg32ui")]
    RWTexture2D<uint2> outProbeGeometryTex;
    [format("rg16f")]
    RWTexture2D<float2> outProbeMotionTex;
    [format("rg32ui")]
    RWTexture2D<uint2> outHistoryProbeGeometryTex;

    SamplerState sampler_point_clamp;
};

[Material]
ParameterBlock<GIProbePackParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 groupID : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    uint2 probeCoord = MortonDecode8x8(groupIndex) + groupID.xy * 8;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);

    float depth = perMaterial.hierarchyDepth.Load(int3(probeFullResCoord, 0)).x;
    float4 normal = perMaterial.normalTex.Load(int3(probeFullResCoord, 0));
    float2 motion = perMaterial.motionVectorTex.Load(int3(probeFullResCoord, 0)).xy;

    float historyDepth = perMaterial.historyDepthTex.Load(int3(probeFullResCoord, 0)).x;
    float4 historyNormal = perMaterial.historyNormalTex.Load(int3(probeFullResCoord, 0));

    perMaterial.outProbeGeometryTex[probeCoord] = PackProbeGeometry(depth, DecodeGBufferNormal(normal.xyz));
    perMaterial.outProbeMotionTex[probeCoord] = motion;
    perMaterial.outHistoryProbeGeometryTex[probeCoord] = PackProbeGeometry(historyDepth, DecodeGBufferNormal(historyNormal.xyz));
}
