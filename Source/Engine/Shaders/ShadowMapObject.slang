import Library.PerScene;
import WeilanEngine;
import Shadows;

#if CONFIG
cull: back
dynamicState:
    - depthBiasEnable
    - depthBias
#endif

static extern const bool _GPUDriven = false;

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

struct VertexOutput
{
    float4 position : SV_Position;
};

ParameterBlock<PerScene> perScene;
ParameterBlock<ShadowPass> shadowPass;

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex, uint renderDataIndex : SV_InstanceID)
{
    float4x4 modelMatrix;

    uint objectOffset = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    GpuObject objData = perScene.LoadData<GpuObject>(objectOffset);
    GpuRenderData renderData = perScene.LoadData<GpuRenderData>(objData.pRenderDataOffset, renderDataIndex);
    GpuGeometry geometry = perScene.LoadData<GpuGeometry>(renderData.geometryOffset);

    modelMatrix = objData.model;
    uint positionOffset = geometry.positionOffset;

    GpuGeometryPositionData positionData = perScene.LoadData<GpuGeometryPositionData>(positionOffset, vertexID);

    float3 positionWS = mul(modelMatrix, float4(positionData.position, 1.0)).xyz;

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow[shadowPass.cascadeIndex], float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}

