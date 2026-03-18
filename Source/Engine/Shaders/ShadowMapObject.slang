import Library.PerScene;
import WeilanEngine;
import Shadows;

#if CONFIG
cull: back
dynamicState:
    - depthBiasEnable
    - depthBias
#endif

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
VertexOutput VertexMain(uint vertexID : SV_VertexID)
{
    uint indexOffset = asuint(transform.invTspModel[0][3]);
    uint positionOffset = asuint(transform.invTspModel[1][3]);

    uint vertexIndex = perScene.LoadData<uint>(indexOffset, vertexID);
    VertexInput input = perScene.LoadData<VertexInput>(positionOffset, vertexIndex);

    float3 positionWS = float3(mul(transform.model, float4(input.position, 1.0)).xyz);

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow[shadowPass.cascadeIndex], float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}
