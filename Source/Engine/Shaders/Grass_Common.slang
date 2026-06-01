struct GrassPatchInstanceData
{
    float3 position;
    float padding;
};

struct GrassParam
{
    float4 grassColorRamp_Bottom;
    float4 grassColorRamp_Top;

    float4 grassColorRamp2_Bottom;
    float4 grassColorRamp2_Top;

    float4 grassColorRamp3_Bottom;
    float4 grassColorRamp3_Top;

    float4 grassMaskUVScaler;

    float hueShift_0;
    float hueShift_1;
    float windScale;
};

struct GrassParams
{
    GrassParam grassParam;
    StructuredBuffer<GrassPatchInstanceData> instanceData;
    Texture2D grassShadowMask0;
    Texture2D grassShadowMask1;
    Texture2D blueNoise;
    Texture2D windMask;
    SamplerState _PointClampSampler;
    SamplerState _LinearClampSampler;
    SamplerState _LinearRepeatSampler;
};

struct GrassPushConstant
{
    float4 albedo;
    uint instanceOffset;
    float scale;
    uint2 padding;
};

struct GrassAttributes
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 properties : TEXCOORD1;
};

float GetGrassInstanceScale(GrassParams params, GrassPatchInstanceData instanceData)
{
    float scale = params.blueNoise.SampleLevel(params._LinearRepeatSampler, instanceData.position.xy / 256, 0).x;
    return lerp(0.9, 1.15, scale);
}

float3 GetGrassBaseWorldPosition(GrassAttributes input, GrassPatchInstanceData instanceData, GrassParams params, GrassPushConstant pconst)
{
    return input.position * pconst.scale * GetGrassInstanceScale(params, instanceData) + instanceData.position;
}

float3 ApplyGrassWind(GrassAttributes input, GrassParams params, float3 positionWS, float time)
{
    positionWS.xz += params.windMask.SampleLevel(params._LinearRepeatSampler, (positionWS.xz + time) / 8.0f, 0).xy * (1 - input.properties.y) * params.grassParam.windScale;
    return positionWS;
}

float3 GetGrassWorldPosition(GrassAttributes input, GrassPatchInstanceData instanceData, GrassParams params, GrassPushConstant pconst, float time)
{
    return ApplyGrassWind(input, params, GetGrassBaseWorldPosition(input, instanceData, params, pconst), time);
}
