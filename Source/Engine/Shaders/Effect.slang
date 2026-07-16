import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

#if CONFIG
name: Effect
cull: none
depth:
    testEnable: true
    writeEnable: false
blend:
    - srcAlpha oneMinusSrcAlpha zero one
#endif

struct MaterialInput
{
    float4 albedo;
    float intensity;

    Texture2D maskMap;
    SamplerState samplerAnisoRepeat;
};

struct TransparentPass
{
    Texture2D depthCopy;
};

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct PushConstant
{
    float4x4 model;
};

[vk_push_constant]
PushConstant pushConstant;

[Global]
ParameterBlock<PerScene> perScene;

[Pass]
ParameterBlock<TransparentPass> pass;

[Material]
ParameterBlock<MaterialInput> material;

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    float3 positionWS = mul(pushConstant.model, float4(input.position, 1.0)).xyz;
    output.position = perScene.WorldToClipSpace(positionWS);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    float mask = material.maskMap.Sample(material.samplerAnisoRepeat, input.uv).r;

    float sceneDepth = pass.depthCopy.Load(int3(input.position.xy, 0)).r;
    float sceneEyeDepth = LinearEyeDepth(sceneDepth, perScene.camera.cameraZBufferParams);
    float effectEyeDepth = LinearEyeDepth(input.position.z, perScene.camera.cameraZBufferParams);
    float intersectionFade = smoothstep(0.0, 0.1, sceneEyeDepth - effectEyeDepth);

    return float4(material.albedo.rgb * material.intensity, mask * material.albedo.a * intersectionFade);
}
