import WeilanEngine;
import Library.PerScene;
import Lighting;
#include "DeferredPBRShadingInput.hlsl"

struct GrassPatchInstanceData
{
    float3 position;
    float padding;
};

struct GrassParams
{
    DeferredPBRShadingInput input;
    StructuredBuffer<GrassPatchInstanceData> instanceData;
    Texture2D shadowMap;
    Texture2D contactShadowMap;
    TextureCube pointLightShadowMap;
    SamplerState _PointClampSampler;
    SamplerState _LinearClampSampler;
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
};

struct Varyings
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
};

[shader("vertex")]
Varyings Vertex(Attributes input, uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation)
{
    GrassPatchInstanceData instanceData = params.instanceData[baseInstance + instanceID];

    Varyings output;
    output.positionWS = input.position * pconst.scale + instanceData.position;
    output.normalWS = normalize(input.normal);
    output.position = perScene.WorldToClipSpace(output.positionWS);
    return output;
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
{
    float2 screenUV = input.position.xy * perScene.camera.screenSize.zw;

    ShadingObject shadingObject;
    shadingObject.positionWS = input.positionWS;
    shadingObject.normalWS = normalize(input.normalWS);
    shadingObject.baseColor = pconst.albedo.xyz;
    shadingObject.viewDir = normalize(perScene.camera.position.xyz - input.positionWS);
    shadingObject.metallic = 0.0f;
    shadingObject.roughness = 0.7f;

    MainLightShadowData mainLightShadowData;
    mainLightShadowData.shadowConstantBias = params.input.shadowConstantBias;
    mainLightShadowData.shadowNormalBias = params.input.shadowNormalBias;
    mainLightShadowData.shadowMap = params.shadowMap;
    mainLightShadowData.shadowSampler = params.samplerShadow_clamp;

    GI gi;
    gi.ao = 1.0f;

    PointLightShadowData pointLightShadowData;
    pointLightShadowData.shadowMap = params.pointLightShadowMap;
    pointLightShadowData.shadowSampler = params.samplerShadow;
    pointLightShadowData.lightIndex = params.input.pointLightShadowLightIndex;
    pointLightShadowData.farPlane = params.input.pointLightShadowFarPlane;
    pointLightShadowData.depthBias = params.input.pointLightShadowLightPosAndBias.w;
    pointLightShadowData.lightPosition = params.input.pointLightShadowLightPosAndBias.xyz;

    float3 finalColor = pconst.albedo.xyz * 0.03f;
    for (int i = 0; i < perScene.scene.lightCount; i++)
    {
        finalColor += StandardLightShading(
            params._LinearClampSampler,
            params._PointClampSampler,
            screenUV,
            perScene.scene.lights[i],
            perScene.camera,
            perScene.mainLightShadow,
            shadingObject,
            mainLightShadowData,
            gi,
            params.contactShadowMap,
            i,
            pointLightShadowData);
    }

    return float4(finalColor, 1.0f);
}
