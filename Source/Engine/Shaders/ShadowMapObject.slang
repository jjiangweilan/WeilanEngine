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

struct Transform
{
    matrix<float, 4,4> model;
    matrix<float, 4,4> invTspModel; // only for global buffer offset data stored in the fourth column
};

[vk::push_constant]
Transform transform;

struct VertexInput
{
    float3 position;
}

struct VertexOutput
{
    float4 position : SV_Position;
}

ParameterBlock<PerScene> perScene;
ParameterBlock<ShadowPass> shadowPass;

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex)
{
    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
    uint indexOffset;
    uint positionOffset;
    uint objectIndex = 0;

    if (_GPUDriven)
    {
        objectIndex = perScene.objectIDs[asuint(transform.model[0][0]) + drawIndex];
        GPUSceneObjectData objData = perScene.LoadData<GPUSceneObjectData>(
            perScene.gpuDrivenConfig.sceneObjectDataBaseOffset, objectIndex);
        modelMatrix = objData.model;
        invTspModelMatrix = objData.invTspModel;
        indexOffset = asuint(objData.invTspModel[0][3]);
        positionOffset = asuint(objData.invTspModel[1][3]);
    }
    else
    {
        modelMatrix = transform.model;
        invTspModelMatrix = transform.invTspModel;
        indexOffset = asuint(transform.invTspModel[0][3]);
        positionOffset = asuint(transform.invTspModel[1][3]);
    }

    uint vertexIndex = perScene.LoadData<uint>(indexOffset, vertexID);
    VertexInput input = perScene.LoadData<VertexInput>(positionOffset, vertexIndex);

    float3 positionWS = float3(mul(modelMatrix, float4(input.position, 1.0)).xyz);

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow[shadowPass.cascadeIndex], float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}

