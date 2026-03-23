import WeilanEngine;
import Library.PerScene;
import Lighting;
#include "Library/Grid.hlsl"

static extern const bool _GPUDriven = true;

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
};

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
    nointerpolation uint objectIndex;
}

[Global]
ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex)
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
        indexOffset = objData.indexOffset;
        positionOffset = objData.positionOffset;
        attributeOffset = objData.attributeOffset;
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
    float3 vertexPos = positionData.position;// input.GetPosition();

    float3 normal = attributeData.normal;//ainput.GetNormal();
    float4 tangent = attributeData.tangent;// input.GetTangent();

    output.positionWS = mul(modelMatrix, float4(vertexPos, 1)).xyz;

    matrix<float,3,3> model33 = (matrix<float,3,3>)invTspModelMatrix;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = attributeData.uv;// input.GetUV();

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    float3 scale = float3(length(modelMatrix[0].xyz), length(modelMatrix[1].xyz), length(modelMatrix[2].xyz));
    output.positionOS = vertexPos * scale;// input.GetPosition() * scale;
    output.objectIndex = objectIndex;

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
