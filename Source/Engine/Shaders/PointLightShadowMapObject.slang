import Library.PerScene;
import WeilanEngine;
import ObjectWorldPosition;
import TerrainCommon;

static extern const bool _Terrain = false;

#if CONFIG
cull: back
dynamicState:
    - depthBiasEnable
    - depthBias
depth:
    testEnable: true
    writeEnable: true
#endif

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

#include "Shadows.hlsl"

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
};

ParameterBlock<PerScene> perScene;
ParameterBlock<PointLightShadowPass> shadowPass;

[shader("vertex")]
VertexOutput VertexMain(uint vertexIndex : SV_VertexID, uint drawIndex : SV_DrawIndex, uint renderDataIndex : SV_InstanceID)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);

    float4x4 modelMatrix = entity.GetModelMatrix();
    GpuMaterial material = entity.GetMaterial(perScene);
    bool hasBoneWeight = false;
    float4x4 objectModelMatrix = GetObjectModelMatrix(perScene, modelMatrix, entity.skeletonOffset, entity.GetBone(), hasBoneWeight);
    float3 localPosition = entity.GetPosition();
    if (_Terrain)
        localPosition = ApplyTerrainDisplacement(perScene, material, localPosition, entity.GetUV());
    float3 positionWS = mul(objectModelMatrix, float4(localPosition, 1.0)).xyz;

    VertexOutput output;
    output.positionWS = positionWS;
    output.position = mul(shadowPass.worldToShadow, float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
float FragmentMain(VertexOutput input) : SV_Depth
{
    float3 fragToLight = input.positionWS - shadowPass.lightPosition;

    return 1 - length(fragToLight) / shadowPass.farPlane;
}
