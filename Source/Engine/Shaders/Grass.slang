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

float3 CalculateGrassSpecularHighlight(
    float3 normalWS,
    float3 bladeDirectionWS,
    float3 viewDirWS,
    float3 lightDirWS,
    float3 lightColor,
    float roughness,
    float intensity)
{
    float3 normal = normalize(normalWS);
    float3 bladeDirection = normalize(bladeDirectionWS);
    float3 viewDir = normalize(viewDirWS);
    float3 lightDir = normalize(lightDirWS);
    float3 halfDir = normalize(viewDir + lightDir);

    float normalLight = saturate(dot(normal, lightDir));
    float normalView = saturate(dot(normal, viewDir));
    float bladeHalf = dot(bladeDirection, halfDir);
    float strandHighlight = sqrt(saturate(1.0f - bladeHalf * bladeHalf));
    float specularPower = lerp(96.0f, 8.0f, saturate(roughness));
    float specular = pow(strandHighlight, specularPower);
    float visibility = sqrt(max(normalLight * normalView, 1.0e-4f));

    return lightColor * specular * visibility * intensity;
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

    float3 maskedColor0 = lerp(color0, color1, mask0);
    float3 maskedColor1 = lerp(color0, color1, mask1);

    float3 final = maskedColor0;//Overlay(color0, color1, 0.5);

    float3 normalWS = normalize(input.normalWS);
    float3 viewDirWS = normalize(input.positionWS - perScene.camera.position.xyz);
    float3 bladeDirectionWS = float3(0.0f, 1.0f, 0.0f);
    float3 specularHighlight = 0.0f;

    for (int i = 0; i < perScene.scene.lightCount; i++)
    {
        Light light = perScene.scene.lights[i];
        float4 lightPos = light.position;
        float3 lightDirWS = lightPos.a == 0.0f ? normalize(lightPos.xyz) : normalize(lightPos.xyz - input.positionWS);
        float falloff = lightPos.a == 0.0f ? 1.0f : PointLightInverseSquareFalloff(distance(lightPos.xyz, input.positionWS), light.pointLightTerm2);

        specularHighlight += CalculateGrassSpecularHighlight(
            normalWS,
            bladeDirectionWS,
            viewDirWS,
            lightDirWS,
            light.lightColor.xyz * light.intensity * falloff,
            0.35f,
            0.15f);
    }

    final += specularHighlight;

    // float hueShift = Remap(input.randomValueIsland, 0, 1, params.grassParam.hueShift_0, params.grassParam.hueShift_0 + params.grassParam.hueShift_1);
    // final = HueSaturationValueRemap(final, hueShift, 1, 1);

    return float4(final, 1.0f);
}
