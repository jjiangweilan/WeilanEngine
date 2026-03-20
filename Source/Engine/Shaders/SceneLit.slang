import WeilanEngine;
import Library.PerScene;
import Lighting;

static extern const bool _AlphaClip = false;
static extern const bool _BaseColorMap = false;
static extern const bool _NormalMap = false;
static extern const bool _EmissiveMap = false;
static extern const bool _MetallicRoughnessMap = false;
static extern const bool _GPUDriven = false;

interface IVertexInput
{
    float3 GetPosition();
    float3 GetNormal();
    float4 GetTangent();
    float2 GetUV();
    float4 GetBone();
}

struct PushConstant
{
    float4x4 model;
    float4x4 invTspModel;
} ;

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
    nointerpolation uint objectIndex;
}

[Global]
ParameterBlock<PerScene> perScene;

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

VertexOutput SceneLitVertex(uint vertexID, uint drawIndex)
{
    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
    uint indexOffset;
    uint positionOffset;
    uint attributeOffset;
    uint objectIndex = 0;

    if (_GPUDriven)
    {
        objectIndex = perScene.objectIDs[asuint(pconst.model[0][0]) + drawIndex];
        GPUSceneObjectData objData = perScene.LoadData<GPUSceneObjectData>(
            perScene.gpuDrivenConfig.sceneObjectDataBaseOffset, objectIndex);
        modelMatrix = objData.model;
        invTspModelMatrix = objData.invTspModel;
        indexOffset = asuint(objData.invTspModel[0][3]);
        positionOffset = asuint(objData.invTspModel[1][3]);
        attributeOffset = asuint(objData.invTspModel[2][3]);
    }
    else
    {
        modelMatrix = pconst.model;
        invTspModelMatrix = pconst.invTspModel;
        indexOffset = asuint(pconst.invTspModel[0][3]);
        positionOffset = asuint(pconst.invTspModel[1][3]);
        attributeOffset = asuint(pconst.invTspModel[2][3]);
    }

    uint vertexIndex = perScene.LoadData<uint>(indexOffset, vertexID);
    SceneObjectPositionData positionData = perScene.LoadData<SceneObjectPositionData>(positionOffset, vertexIndex);
    SceneObjectAttributeData attributeData = perScene.LoadData<SceneObjectAttributeData>(attributeOffset, vertexIndex);

    VertexOutput output;
    float3 vertexPos = positionData.position;
    float3 normal = attributeData.GetNormal();
    float4 tangent = attributeData.GetTangent();

    output.positionWS = mul(modelMatrix, float4(vertexPos, 1)).xyz;

    matrix<float,3,3> model33 = (float3x3)invTspModelMatrix;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = attributeData.GetUV();
    output.objectIndex = objectIndex;

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    return output;
}

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex)
{
    return SceneLitVertex(vertexID, drawIndex);
}

FragmentOutput SceneLitFragment(VertexOutput input)
{
    FragmentOutput output;

    float4 baseColor;
    float metallic;
    float roughness;
    float alphaCutoff;
    float3 normalSample;
    float4 emission;

    if (_GPUDriven)
    {
        GPUSceneObjectData objData = perScene.LoadData<GPUSceneObjectData>(
            perScene.gpuDrivenConfig.sceneObjectDataBaseOffset, input.objectIndex);
        GPUMaterialData matData = perScene.LoadData<GPUMaterialData>(
            perScene.gpuDrivenConfig.materialDataBaseOffset, objData.materialIndex);

        baseColor = matData.baseColorFactor;
        if (matData.baseColorTexIndex != 0xFFFFFFFF)
        {
            baseColor *= perScene.globalTextures[matData.baseColorTexIndex].Sample(
                perScene.sampler_linear_clamp, input.uv);
        }

        if (_AlphaClip)
        {
            if (baseColor.a < matData.alphaCutoff)
                discard;
        }

        metallic = matData.metallic;
        roughness = matData.roughness;
        if (matData.metallicRoughnessTexIndex != 0xFFFFFFFF)
        {
            float2 metallicRoughness = float2(roughness, metallic) *
                perScene.globalTextures[matData.metallicRoughnessTexIndex].Sample(
                    perScene.sampler_linear_clamp, input.uv).gb;
            metallic = metallicRoughness.y;
            roughness = clamp(metallicRoughness.x, 0.01, 0.99f);
        }
        else
            roughness = clamp(roughness, 0.01, 0.99f);

        normalSample = float3(0, 0, 1);
        if (matData.normalMapTexIndex != 0xFFFFFFFF)
        {
            normalSample = perScene.globalTextures[matData.normalMapTexIndex].Sample(
                perScene.sampler_linear_clamp, input.uv).xyz * 2 - 1;
        }

        emission = float4(0, 0, 0, 0);
        if (matData.emissiveMapTexIndex != 0xFFFFFFFF)
        {
            emission = perScene.globalTextures[matData.emissiveMapTexIndex].Sample(
                perScene.sampler_linear_clamp, input.uv);
        }
        emission *= matData.emissive;
    }
    else
    {
        baseColor = perMaterial.baseColorFactor;
        if (_BaseColorMap)
        {
            baseColor *= perMaterial.baseColorTex.Sample(input.uv);
        }

        if (_AlphaClip)
        {
            if (baseColor.a < perMaterial.alphaCutoff)
                discard;
        }

        metallic = perMaterial.metallic;
        roughness = perMaterial.roughness;
        if (_MetallicRoughnessMap)
        {
            float2 metallicRoughness = float2(roughness, metallic) * perMaterial.metallicRoughnessMap.Sample(input.uv).gb;
            metallic = metallicRoughness.y;
            roughness = clamp(metallicRoughness.x, 0.01, 0.99f);
        }
        else
            roughness = clamp(roughness, 0.01, 0.99f);

        normalSample = float3(0, 0, 1);
        if (_NormalMap)
        {
            normalSample = perMaterial.normalMap.Sample(input.uv).xyz * 2 - 1;
        }

        emission = float4(0, 0, 0, 0);
        if (_EmissiveMap)
        {
            emission = perMaterial.emissiveMap.Sample(input.uv);
        }
        emission *= perMaterial.emissive;
    }

    float3x3 TBN = transpose(float3x3(normalize(input.tangentWS), normalize(input.bitangentWS), normalize(input.normalWS)));

    float3 n = EncodeGBufferNormal(normalize(mul(TBN, normalSample)));

    output.color = float4(emission.xyz, 1.0f);
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
