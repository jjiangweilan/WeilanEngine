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
VertexOutput VertexMain(uint vertexIndex : SV_VertexID, uint drawIndex : SV_DrawIndex, uint renderDataIndex : SV_InstanceID)
{
    float4x4 modelMatrix;

    uint objectOffset = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, objectOffset, renderDataIndex, vertexIndex);

    modelMatrix = entity.GetModelMatrix();

    float3 positionWS = mul(modelMatrix, float4(entity.GetPosition(), 1.0)).xyz;

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow[shadowPass.cascadeIndex], float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}

