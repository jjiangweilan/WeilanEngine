import WeilanEngine;
import Library.PerScene;
import Skeleton;
import Lighting;

static extern const bool _AlphaTest = false;
static extern const bool _BaseColorMap = false;
static extern const bool _NormalMap = false;
static extern const bool _EmissiveMap = false;
static extern const bool _MetallicRoughnessMap = false;

interface IVertexInput
{
    float3 GetPosition();
    float3 GetNormal();
    float4 GetTangent();
    float2 GetUV();
    float4 GetBone();
}

struct SkinnedVertexInput : IVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 bone : BONE;

    float3 GetPosition() {return position;}
    float3 GetNormal() {return normal;}
    float4 GetTangent() {return tangent;}
    float2 GetUV() { return uv;}
    float4 GetBone() { return bone;}
}

extern struct VertexInput : IVertexInput = SkinnedVertexInput;

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
    float2 uv;
};

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

struct PerObject
{
    ConstantBuffer<Skeleton> skeleton;
}

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<PerMaterial> perMaterial;
[Object]
ParameterBlock<PerObject> perObject;

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
    float4 property : SV_Target3;
}

VertexOutput SceneLitVertex(IVertexInput input)
{
    VertexOutput output;

// calculate bone position
    float4 boneWeights = input.GetBone();

    float4x4 skeletonMatrix = float4x4(0);
    for(int i = 0; i < 4; ++i)
    {
        float boneWeight = boneWeights[i];
        if (boneWeight != 0)
        {
            int boneIndex = int(boneWeight * 0.1);
            float weight = boneWeight - boneIndex * 10;
            skeletonMatrix += perObject.skeleton.bones[boneIndex] * weight;
        }
    }

    float3 position = input.GetPosition();
    float4 vertexPos = mul(skeletonMatrix, float4(position, 1));
    float3 normal = input.GetNormal();
    float4 tangent = input.GetTangent();

    output.positionWS = vertexPos.xyz;//mul(pconst.model, float4(vertexPos.xyz, 1)).xyz;

    matrix<float,3,3> model33 = (float3x3)skeletonMatrix;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = input.GetUV();

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    return output;
}


[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    return SceneLitVertex(input);
}


FragmentOutput SceneLitFragment(VertexOutput input)
{
    FragmentOutput output;

    float4 baseColor = perMaterial.baseColorFactor;
    if (_BaseColorMap)
    {
        baseColor *= perMaterial.baseColorTex.Sample(input.uv);
    }

    if (_AlphaTest)
    {
        if (baseColor.a < perMaterial.alphaCutoff)
            discard;
    }

    float metallic = perMaterial.metallic;
    float roughness = perMaterial.roughness;
    if (_MetallicRoughnessMap)
    {
        float2 metallicRoughness = float2(roughness, metallic) * perMaterial.metallicRoughnessMap.Sample(input.uv).gb;
        metallic = metallicRoughness.y;
        roughness = max(metallicRoughness.x, 0.01);
    }

    float3 normal = float3(0,0,1);
    if (_NormalMap)
    {
        normal = perMaterial.normalMap.Sample(input.uv).xyz * 2 - 1;
    }
    float3x3 TBN = transpose(float3x3(normalize(input.tangentWS), normalize(input.bitangentWS), normalize(input.normalWS)));

    float3 n = EncodeGBufferNormal(normalize(mul(TBN, normal)));

    float4 emission = 0;
    if (_EmissiveMap)
    {
        emission = perMaterial.emissiveMap.Sample(input.uv);
    }

    output.color = float4(emission.xyz * perMaterial.emissive.xyz, 1.0f);
    output.albedo = float4(baseColor.xyz, 1.0f);
    output.normal = float4(n, 1.0f);
    output.property = float4(metallic, roughness, 0, 1.0f);

    return output;
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input);
}

// struct GIBakeOutput
// {
//     float4 albedo : SV_Target0;
//     float4 normal : SV_Target1;
// }
// 
// struct ProbeInfo
// {
//     float4 position;
// };
// 
// [vk_binding(0,3)]
// ParameterBlock<ProbeInfo> probeInfos;
// 
// [shader("fragment")]
// GIBakeOutput GIMicroGBuffer(VertexInput input)
// {
//     GIBakeOutput output;
//     output.albedo = float4(0);
//     output.normal = float4(0);
// 
//     return output;
// }
