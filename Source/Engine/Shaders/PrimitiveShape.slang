import WeilanEngine;
import Library.PerScene;
#include "DeferredPBRShading.hlsl"
#include "Library/Grid.hlsl"

interface IVertexInput
{
    float3 GetPosition();
    float3 GetNormal();
    float4 GetTangent();
    float2 GetUV();
    float4 GetBone();
}

struct StandardVertexInput : IVertexInput
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;

    float3 GetPosition() {return position;}
    float3 GetNormal() {return normal;}
    float4 GetTangent() {return tangent;}
    float2 GetUV() { return uv;}
    float4 GetBone() { return 0;}
}

extern struct VertexInput : IVertexInput = StandardVertexInput;

struct PushConstant
{
    float4x4 model;
    float4x4 invTspModel;
} 

[vk_push_constant]
PushConstant pconst;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangentWS;
    float3 positionOS; 
    float2 uv;
}

[Global]
ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;
    float3 vertexPos = input.GetPosition();
    float3 normal = input.GetNormal();
    float4 tangent = input.GetTangent();

    output.positionWS = mul(pconst.model, float4(vertexPos, 1)).xyz;

    matrix<float,3,3> model33 = (matrix<float,3,3>)pconst.invTspModel;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = input.GetUV();

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    float3 scale = float3(length(pconst.model[0].xyz), length(pconst.model[1].xyz), length(pconst.model[2].xyz));
    output.positionOS = input.GetPosition() * scale;

    return output;
}

struct PerMaterial
{
    float4 baseColorFactor;
    float4 emissive;
    float roughness;
    float metallic;
    float alphaCutoff;

    Sampler2D baseColorTex;
    Sampler2D normalMap;
    Sampler2D metallicRoughnessMap;
    Sampler2D emissiveMap;
};

[Material]
ParameterBlock<PerMaterial> perMaterial;

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
    float4 property : SV_Target3;
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    FragmentOutput output;

    float grid = PristineGrid3D(input.positionOS, 0.01f);
    float4 baseColor = float4(float3(lerp(0.5, 1.0, grid)), 1.0);

    float3 normal = float3(0,0,1);
    float3x3 TBN = float3x3(normalize(input.tangentWS), normalize(input.bitangentWS), normalize(input.normalWS));
    TBN = transpose(TBN);

    float3 n = EncodeGBufferNormal(normalize(mul(TBN, normal)));
    float4 emission = float4(0);

    output.color = float4(emission.xyz, 1.0f);
    output.albedo= baseColor;
    output.normal = float4(n, 0);
    output.property = float4(0.01f, 1.0f, 1, 0);

    return output;
}
