import WeilanEngine;
import Library.PerScene;
import Lighting;

struct GrassPatchInstanceData
{
    float3 position;
    float padding;
};

struct GrassParams
{
    StructuredBuffer<GrassPatchInstanceData> instanceData;
    SamplerState _PointClampSampler;
    SamplerState _LinearClampSampler;
};

struct PushConstant
{
    uint instanceOffset;
    float3 albedo;
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
    output.positionWS = input.position + instanceData.position;
    output.normalWS = normalize(input.normal);
    output.position = perScene.WorldToClipSpace(output.positionWS);
    return output;
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
{
    float2 screenUV = 0.0f;
    if (perScene.camera.screenSize.x > 0 && perScene.camera.screenSize.y > 0)
    {
        screenUV = input.position.xy * perScene.camera.screenSize.zw;
    }

    ShadingObject shadingObject;
    shadingObject.positionWS = input.positionWS;
    shadingObject.normalWS = normalize(input.normalWS);
    shadingObject.baseColor = pconst.albedo;
    shadingObject.viewDir = normalize(perScene.camera.position.xyz - input.positionWS);
    shadingObject.metallic = 0.0f;
    shadingObject.roughness = 0.7f;

    float3 finalColor = pconst.albedo * 0.03f;
    for (int i = 0; i < perScene.scene.lightCount; i++)
    {
        finalColor += StandardLightShading_DirectLightOnly(
            params._LinearClampSampler,
            params._PointClampSampler,
            screenUV,
            perScene.scene.lights[i],
            perScene.mainLightShadow,
            shadingObject);
    }

    return float4(finalColor, 1.0f);
}
