import WeilanEngine;
import Library.PerScene;
import Lighting;
import Library.CommonUtils;

#if CONFIG
stencil:
    testEnable: true
    writeMask: 0xFF
    reference: 2
    passOp: replace
    compareOp: always
#endif

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

struct PushConstant
{
    float4 albedo;
    uint instanceOffset;
    float scale;
    uint2 padding;
};

[Global]
ParameterBlock<PerScene> perScene;

[Object]
ParameterBlock<GrassParams> params;

[vk_push_constant]
PushConstant pconst;

struct Attributes
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 properties : TEXCOORD1;
};

struct Varyings
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    int colorSelection;
};

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
    float4 property : SV_Target3;
};

float3 Overlay(float3 color0, float3 colro1, float factor)
{
    float3 low = 2.0f * color0 * colro1;
    float3 high = 1.0f - 2.0f * (1.0f - color0) * (1.0f - colro1);
    float3 overlay = lerp(low, high, step(float3(0.5f, 0.5f, 0.5f), color0));
    return saturate(lerp(color0, overlay, saturate(factor)));
}

[shader("vertex")]
Varyings Vertex(Attributes input, uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation)
{
    GrassPatchInstanceData instanceData = params.instanceData[baseInstance + instanceID];

    float scale = params.blueNoise.SampleLevel(params._LinearRepeatSampler, instanceData.position.xy / 256, 0).x;
    scale = lerp(0.9, 1.15, scale);

    Varyings output;
    float3 positionWS = input.position * pconst.scale * scale + instanceData.position;
    positionWS.xz += params.windMask.SampleLevel(params._LinearRepeatSampler, (positionWS.xz + perScene.scene.time) / 8.0f, 0).xy * (1 - input.properties.y) * params.grassParam.windScale;

    output.positionWS = positionWS;
    output.normalWS = normalize(input.normal);
    output.position = perScene.WorldToClipSpace(output.positionWS);
    output.colorSelection = int(input.properties.x) % 3;

    return output;
}

float AdjustContrast(float value, float contrast)
{
    return saturate((value - 0.5) * contrast + 0.5);
}

[shader("fragment")]
FragmentOutput Fragment(Varyings input)
{
    float3 pos = input.positionWS;
    float mask0 = AdjustContrast(params.grassShadowMask0.Sample(params._LinearRepeatSampler, (pos.xz + perScene.scene.time / 16) * params.grassParam.grassMaskUVScaler.xy).x, 2);
    float mask1 = AdjustContrast(params.grassShadowMask1.Sample(params._LinearRepeatSampler, (pos.xz + perScene.scene.time / 8) * params.grassParam.grassMaskUVScaler.zw).x, 2);

    float3 color0 = 0;
    float3 color1 = 0;
    if (input.colorSelection == 0)
    {
        color0 = params.grassParam.grassColorRamp_Bottom.xyz;
        color1 = params.grassParam.grassColorRamp_Top.xyz;
    }
    else if (input.colorSelection == 1)
    {
        color0 = params.grassParam.grassColorRamp2_Bottom.xyz;
        color1 = params.grassParam.grassColorRamp2_Top.xyz;
    }
    else
    {
        color0 = params.grassParam.grassColorRamp3_Bottom.xyz;
        color1 = params.grassParam.grassColorRamp3_Top.xyz;
    }

    float3 maskedColor0 = lerp(color0, color1, mask0);
    float3 albedo = maskedColor0;

    FragmentOutput output;
    output.color = float4(0, 0, 0, 0);
    output.albedo = float4(albedo, 1.0f);
    output.normal = float4(EncodeGBufferNormal(normalize(input.normalWS)), 0);
    output.property = float4(0, 0, 0, 0);
    return output;
}