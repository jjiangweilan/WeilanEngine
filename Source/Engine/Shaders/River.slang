import WeilanEngine;
import Library.PerScene;

#if CONFIG
cull: none
blend:
- one oneMinusSrcAlpha
depth:
    testEnable: true
    writeEnable: false
#endif

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangetWS;
    float2 uv;
}

struct Ps
{
    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
}

struct Material
{
    [Texture] Texture2D normalMap;

    SamplerState _Linear_Sampler;
}

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<Material> mat;

[vk_push_constant]
Ps pushConstants;

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    float3 positionWS = mul(pushConstants.modelMatrix, float4(input.position, 1.0)).xyz;
    float3 normalWS = mul((float3x3)pushConstants.invTspModelMatrix, input.normal);
    float3 tangentWS = mul((float3x3)pushConstants.invTspModelMatrix, input.tangent.xyz);
    float4 ndc = perScene.WorldToClipSpace(positionWS);

    VertexOutput output;
    output.position = ndc;
    output.positionWS = positionWS;
    output.normalWS = normalWS;
    output.tangentWS = tangentWS;
    output.bitangetWS = cross(normalWS, tangentWS);
    output.uv = input.uv;

    return output;

}


[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    float3 normalMap = normalize(mat.normalMap.Sample(mat._Linear_Sampler, input.uv).xyz);
    float3 normalWS = mul(transpose(float3x3(normalize(input.tangentWS), normalize(input.bitangetWS), normalize(input.normalWS))), normalMap);
    Light mainLight = perScene.GetMainLight();
    float ndotl = dot(normalWS, mainLight.position.xyz);
    return float4(0,0,1,0.5) * ndotl;
}
