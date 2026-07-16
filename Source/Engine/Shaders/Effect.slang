import WeilanEngine;
import Library.PerScene;

#if CONFIG
name: Effect
cull: none
depth:
    testEnable: true
    writeEnable: false
blend:
    - srcAlpha oneMinusSrcAlpha
#endif

[MaterialUILayout]
struct EffectUI
{
    [Color]
    float4 albedo;

    [Texture]
    int maskMap;
};

struct MaterialInput
{
    float4 albedo;
    Texture2D maskMap;
    SamplerState samplerLinearRepeat;
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
    float mask = material.maskMap.Sample(material.samplerLinearRepeat, input.uv).r;
    return float4(material.albedo.rgb, mask * material.albedo.a);
}
