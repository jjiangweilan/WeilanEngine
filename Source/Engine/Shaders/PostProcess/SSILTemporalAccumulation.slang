import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;

struct SSILTemporalAccumulationParameter
{
    float4 rtSize; // xy: width/height, zw: 1/width/height
    float historyWeight;
    float depthTolerance;
    float normalThreshold;
    float padding0;

    Texture2D<float4> currentSsilTex;
    Texture2D<float2> motionVectorTex;
    Texture2D hierarchyDepthTex;
    Texture2D normalTex;
    Texture2D<float4> historySsilTex;
    Texture2D<float> historyDepthTex;
    Texture2D historyNormalTex;

    RWTexture2D<float4> outSsilTex;

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILTemporalAccumulationParameter> perMaterial;

float3 ReconstructWorldPos(float2 uv, float depth)
{
    float4 positionWS = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, depth, 1.0));
    return positionWS.xyz / positionWS.w;
}

float3 ReconstructPreviousWorldPos(float2 uv, float depth)
{
    float4 positionWS = mul(perScene.camera.invPreviousViewProjection, float4(uv * 2.0 - 1.0, depth, 1.0));
    return positionWS.xyz / positionWS.w;
}

void GetNeighborhoodMinMax(uint2 coord, out float4 outMinValue, out float4 outMaxValue)
{
    uint2 size = uint2(perMaterial.rtSize.xy);
    outMinValue = float4(65504.0, 65504.0, 65504.0, 65504.0);
    outMaxValue = -outMinValue;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            uint2 sampleCoord = clamp(int2(coord) + int2(x, y), int2(0, 0), int2(size) - 1);
            float4 value = perMaterial.currentSsilTex.Load(int3(sampleCoord, 0));
            outMinValue = min(outMinValue, value);
            outMaxValue = max(outMaxValue, value);
        }
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    uint2 size = uint2(perMaterial.rtSize.xy);
    if (coord.x >= size.x || coord.y >= size.y)
        return;

    float2 uv = (float2(coord) + 0.5) * perMaterial.rtSize.zw;
    float4 currentSsil = perMaterial.currentSsilTex.Load(int3(coord, 0));
    float currentDepth = perMaterial.hierarchyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).x;

    if (currentDepth <= 0.0000001)
    {
        perMaterial.outSsilTex[coord] = currentSsil;
        return;
    }

    float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xy;
    float2 prevUv = uv - motion;
    if (any(prevUv < 0.0) || any(prevUv > 1.0))
    {
        perMaterial.outSsilTex[coord] = currentSsil;
        return;
    }

    float historyDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, prevUv, 0).x;
    if (historyDepth <= 0.0000001)
    {
        perMaterial.outSsilTex[coord] = currentSsil;
        return;
    }

    float3 currentNormal = normalize(DecodeGBufferNormal(perMaterial.normalTex.Load(int3(coord, 0)).xyz));
    float3 historyNormal = normalize(DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, prevUv, 0).xyz));
    float3 currentPos = ReconstructWorldPos(uv, currentDepth);
    float3 historyPos = ReconstructPreviousWorldPos(prevUv, historyDepth);

    float positionTolerance = max(perMaterial.depthTolerance * abs(mul(perScene.camera.view, float4(currentPos, 1.0)).z), 1e-4);
    bool validHistory = length(historyPos - currentPos) <= positionTolerance && dot(currentNormal, historyNormal) >= perMaterial.normalThreshold;
    if (!validHistory)
    {
        perMaterial.outSsilTex[coord] = currentSsil;
        return;
    }

    float4 neighborhoodMin;
    float4 neighborhoodMax;
    GetNeighborhoodMinMax(coord, neighborhoodMin, neighborhoodMax);

    float4 historySsil = perMaterial.historySsilTex.SampleLevel(perMaterial.sampler_linear_clamp, prevUv, 0);
    historySsil = clamp(historySsil, neighborhoodMin, neighborhoodMax);

    perMaterial.outSsilTex[coord] = lerp(currentSsil, historySsil, saturate(perMaterial.historyWeight));
}
