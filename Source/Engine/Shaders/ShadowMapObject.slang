import Library.PerScene;
import WeilanEngine;
import Shadows;
import ObjectWorldPosition;

#if CONFIG
cull: back
dynamicState:
    - depthBiasEnable
    - depthBias
#endif

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
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);

    float4x4 modelMatrix = entity.GetModelMatrix();
    bool hasBoneWeight = false;
    float4x4 objectModelMatrix = GetObjectModelMatrix(perScene, modelMatrix, entity.skeletonOffset, entity.GetBone(), hasBoneWeight);

    float3 positionWS = mul(objectModelMatrix, float4(entity.GetPosition(), 1.0)).xyz;

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow[shadowPass.cascadeIndex], float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}
