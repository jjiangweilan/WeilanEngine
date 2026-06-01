import WeilanEngine;
import Library.PerScene;
import Lighting;
import Library.CommonUtils;

#include "Grass_Common.slang"

#if CONFIG
stencil:
    testEnable: true
    writeMask: 0xFF
    reference: 2
    passOp: replace
    compareOp: always
#endif

[Global]
ParameterBlock<PerScene> perScene;

[Object]
ParameterBlock<GrassParams> params;

[vk_push_constant]
GrassPushConstant pconst;

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
Varyings Vertex(GrassAttributes input, uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation)
{
    GrassPatchInstanceData instanceData = params.instanceData[baseInstance + instanceID];

    Varyings output;
    float3 positionWS = GetGrassWorldPosition(input, instanceData, params, pconst, perScene.scene.time);

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
