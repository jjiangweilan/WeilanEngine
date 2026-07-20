import WeilanEngine;
import Library.PerScene;
import TerrainCommon;
import Lighting;

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 positionLS;
    float3 geometryNormalLS;
    float2 terrainUV;
    nointerpolation uint objectOffset;
    nointerpolation uint renderDataIndex;
};

[Global]
ParameterBlock<PerScene> perScene;

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
    float4 property : SV_Target3;
};

[shader("vertex")]
VertexOutput VertexMain(uint vertexIndex : SV_VertexID, uint drawIndex : SV_DrawIndex, uint geometryOffset : SV_InstanceID)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);
    GpuMaterial material = entity.GetMaterial(perScene);

    float3 localPosition = ApplyTerrainDisplacement(
        perScene,
        material,
        entity.GetPosition(),
        entity.GetUV()
    );
    float3 localNormal = GetTerrainLocalNormal(perScene, material, entity.GetUV());
    float4x4 modelMatrix = entity.GetModelMatrix();
    float3 worldPosition = mul(modelMatrix, float4(localPosition, 1.0f)).xyz;

    VertexOutput output;
    output.position = mul(perScene.camera.viewProjection, float4(worldPosition, 1.0f));
    output.positionWS = worldPosition;
    output.positionLS = localPosition;
    output.geometryNormalLS = localNormal;
    output.terrainUV = entity.GetUV();
    output.objectOffset = drawExtra.objectOffset;
    output.renderDataIndex = drawExtra.renderDataIndex;
    return output;
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    GpuObject objectData = perScene.LoadData<GpuObject>(input.objectOffset);
    GpuRenderData renderData = perScene.LoadData<GpuRenderData>(objectData.pRenderDataOffset, input.renderDataIndex);
    GpuMaterial material = perScene.LoadData<GpuMaterial>(renderData.materialOffset);
    GpuTerrainMaterialData terrain = LoadTerrainMaterialData(perScene, material);
    float3 viewDirectionWS = normalize(perScene.camera.position.xyz - input.positionWS);
    float3 viewDirectionLS = normalize(mul((float3x3)transpose(objectData.invTspModel), viewDirectionWS));
    TerrainSurfaceData surface;
    EvaluateTerrainLayers(
        perScene,
        material,
        terrain,
        input.terrainUV,
        input.positionLS,
        normalize(input.geometryNormalLS),
        viewDirectionLS,
        surface
    );
    float3 normalWS = normalize(mul((float3x3)objectData.invTspModel, surface.normalLS));

    FragmentOutput output;
    output.color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    output.albedo = float4(surface.albedo, 1.0f);
    output.normal = float4(EncodeGBufferNormal(normalWS), 1.0f);
    output.property = float4(surface.metallic, clamp(surface.roughness, 0.01f, 0.99f), 0.0f, 1.0f);
    return output;
}
