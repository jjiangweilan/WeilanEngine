import WeilanEngine;
import Library.PerScene;
import Lighting;
#include "Library/Grid.hlsl"

static extern const bool _GPUDriven = true;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangentWS;
    float3 positionOS;
}

struct PushConstant
{
    uint firstGpuObjectOffset;
};
[vk_push_constant]
PushConstant pconst;

[Global]
ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput VertexMain(uint vertexIndex : SV_VertexID, uint drawIndex : SV_DrawIndex, uint renderDataIndex : SV_InstanceID)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);
    float4x4 modelMatrix = entity.GetModelMatrix();
    float4x4 invTspModelMatrix = entity.GetInvModelMatrix();

    VertexOutput output;
    float3 vertexPos = entity.GetPosition();
    float3 normal = entity.GetNormal();
    float4 tangent = entity.GetTangent();

    output.positionWS = mul(modelMatrix, float4(vertexPos, 1)).xyz;

    float3x3 model33 = (float3x3)invTspModelMatrix;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    float3 scale = float3(length(modelMatrix[0].xyz), length(modelMatrix[1].xyz), length(modelMatrix[2].xyz));
    output.positionOS = vertexPos * scale;

    return output;
}

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
