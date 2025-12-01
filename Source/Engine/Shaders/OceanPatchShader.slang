import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;

#include "GerstnerWave.hlsl"

// should match the declaration in OceanRenderer
struct GPUNodeInstanceData
{
    float2 position;
    float2 scale;
};

struct RendererInputBuffer
{
    float4 depthTexSize;
};

struct InputParams
{
    ConstantBuffer<RendererInputBuffer> buffer;
    StructuredBuffer<GPUNodeInstanceData> instanceData;
    Texture2D depthTex;
    Texture2D colorTex;
    TextureCube specularCubemap;
    SamplerState _PointClampSampler;
    SamplerState _LinearClampSampler;
};

struct Attributes
{
    float3 position : POSITION;
};

struct Varyings
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 waveOffset;
};

struct DrawParams
{
    int lod;
};

struct MaterialInput
{
    float4 waveColorBlend;
    [[Color]] float4 waveTopColor;
    [[Color]] float4 waveBottomColor;
    float areaScale;
    float yOffset;
    float fodDensity;
    int waveCount;
    StructuredBuffer<Wave> waves;
}

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<MaterialInput> mat;

[Object]
ParameterBlock<InputParams> params;

[vk_push_constant]
DrawParams drawParams;

[shader("vertex")]
Varyings Vertex(Attributes input, int instanceID :  SV_InstanceID, int baseInstance : SV_StartInstanceLocation)
{
    var instanceData = params.instanceData[baseInstance + instanceID];

    float3 positionWS = input.position;
    positionWS.y = mat.yOffset;
    positionWS.xz *= instanceData.scale;
    positionWS.xz += instanceData.position;

    float time = perScene.scene.time;
    float3 normalWS = float3(0,0,0);
    float3 waveOffset = float3(0,0,0);
    positionWS = GerstnerWave(positionWS, mat.waves, mat.waveCount, mat.areaScale, time, normalWS, waveOffset);

    float4 positionCS = perScene.WorldToClipSpace(positionWS);

    Varyings output;
    output.position = positionCS;
    output.positionWS = positionWS;
    output.normalWS = normalWS;
    output.waveOffset = waveOffset;
    return output;
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
{
    float2 screenUV = (input.position.xy + 0.5) * params.buffer.depthTexSize.zw;

    // Calculate fog
    float sceneDepth = params.depthTex.SampleLevel(params._PointClampSampler, screenUV, 0).r;
    float sceneDepthVS = LinearEyeDepth(sceneDepth, perScene.camera.cameraZBufferParams);
    float depthVS = LinearEyeDepth(input.position.z, perScene.camera.cameraZBufferParams);
    float depthDiff = max(0, sceneDepthVS - depthVS);
    float fog = saturate(exp2(-depthDiff * mat.fodDensity));

    // Ocean material properties
    float3 baseColor = float3(0.02, 0.15, 0.3); // Deep ocean blue
    float metallic = 0.0; // Water is non-metallic
    float roughness = 0.05; // Very smooth surface
    float subsurfaceStrength = 0.3;
    float3 waveOffset = input.waveOffset;

    // Lighting accumulation
    float3 finalColor = float3(0, 0, 0);

    // Process each light
    float colorBlend = saturate(waveOffset.y / (mat.waveColorBlend.y - mat.waveColorBlend.x));
    float3 baseBlendColor = lerp(mat.waveBottomColor.xyz, mat.waveTopColor.xyz, colorBlend);

    ShadingObject shadingObject;
    shadingObject.positionWS = input.positionWS;
    shadingObject.normalWS = normalize(input.normalWS);
    shadingObject.baseColor = baseBlendColor;
    shadingObject.viewDir = normalize(perScene.camera.position.xyz - input.positionWS.xyz);
    shadingObject.metallic = 0.0f;
    shadingObject.roughness = 0.1f;
    for (int i = 0; i < perScene.scene.lightCount; i++)
    {
        Light light = perScene.scene.lights[i];
        float4 lightPos = light.position;

        float3 fresnel = Fresnel_Schlick(dot(shadingObject.normalWS, shadingObject.viewDir), 0);
        float specularCubemapBias = 0.5;
        finalColor += fresnel * params.specularCubemap.SampleLevel(params._LinearClampSampler, shadingObject.normalWS, specularCubemapBias).xyz + 
        StandardLightShading_DirectLightOnly(
        params._LinearClampSampler,
        params._PointClampSampler,
        screenUV,
        perScene.scene.lights[i],
        perScene.mainLightShadow, shadingObject);
    }

    // transmitted light from bottom of the ocean
    float3 sceneColor = params.colorTex.SampleLevel(params._LinearClampSampler, screenUV, 0).xyz;
    float3 transmitted = sceneColor * fog;

    return float4(finalColor + transmitted, 1.0);
}
