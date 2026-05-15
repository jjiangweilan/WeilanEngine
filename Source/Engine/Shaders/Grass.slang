import WeilanEngine;
import Library.PerScene;
import Lighting;
import Library.CommonUtils;
#include "DeferredPBRShadingInput.hlsl"

struct GrassPatchInstanceData
{
    float3 position;
    float padding;
};

struct GrassParam
{
    DeferredPBRShadingInput input;
    float4 grassColorRamp_Bottom;
    float4 grassColorRamp_Top;

    float4 grassColorRamp2_Bottom;
    float4 grassColorRamp2_Top;

    float4 grassColorRamp3_Bottom;
    float4 grassColorRamp3_Top;

    float4 grassMaskUVScaler;

    float hueShift_0;
    float hueShift_1;
};

struct GrassParams
{
    GrassParam grassParam;
    StructuredBuffer<GrassPatchInstanceData> instanceData;
    Texture2D shadowMap;
    Texture2D contactShadowMap;
    Texture2D grassShadowMask0;
    Texture2D grassShadowMask1;
    Texture2D blueNoise;
    TextureCube pointLightShadowMap;
    SamplerState _PointClampSampler;
    SamplerState _LinearClampSampler;
    SamplerState _LinearRepeatSampler;
    SamplerComparisonState samplerShadow_clamp;
    SamplerComparisonState samplerShadow;
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
    float2 islandIndex : TEXCOORD1;
};

struct Varyings
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    int colorSelection;
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
    output.positionWS = input.position * pconst.scale * scale + instanceData.position;
    output.normalWS = normalize(input.normal);
    output.position = perScene.WorldToClipSpace(output.positionWS);
    output.colorSelection = int(input.islandIndex.x) % 3;
    return output;
}

float AdjustContrast(float value, float contrast)
{
    // value: grayscale value in [0, 1]
    // contrast: 1.0 = unchanged
    //           >1.0 = higher contrast
    //           <1.0 = lower contrast

    return saturate((value - 0.5) * contrast + 0.5);
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
{
    float3 pos = input.positionWS;
    float mask0 = AdjustContrast(params.grassShadowMask0.Sample(params._LinearRepeatSampler, (pos.xz+ perScene.scene.time / 16) * params.grassParam.grassMaskUVScaler.xy).x, 2);
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

    Light mainLight = perScene.GetMainLight();

    float3 maskedColor0 = lerp(color0, color1, mask0);
    float3 maskedColor1 = lerp(color0, color1, mask1);

    float3 albedo = maskedColor0;// * mainLight.lightColor.xyz * mainLight.intensity * 0.1;//Overlay(color0, color1, 0.5);

    return float4(albedo, 1.0f);
}
