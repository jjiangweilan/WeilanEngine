import WeilanEngine;
import Library.PerScene;

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: equal
#endif

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

[Global]
ParameterBlock<PerScene> perScene;

struct DynamicMotionVectorPassData
{
    StructuredBuffer<float4x4> dynamicMotionPreviousModels;
};

[Material]
ParameterBlock<DynamicMotionVectorPassData> passData;

struct VertexOutput
{
    float4 position : SV_Position;
    float4 currentClip;
    float4 previousClip;
};

[shader("vertex")]
VertexOutput VertexMain(uint vertexIndex : SV_VertexID, uint drawIndex : SV_DrawIndex)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);

    float4 currentWorld = mul(entity.GetModelMatrix(), float4(entity.GetPosition(), 1.0));
    float4 previousWorld = mul(passData.dynamicMotionPreviousModels[drawIndex], float4(entity.GetPosition(), 1.0));

    VertexOutput output;
    output.currentClip = mul(perScene.camera.viewProjection, currentWorld);
    output.previousClip = mul(perScene.camera.previousViewProjection, previousWorld);
    output.position = output.currentClip;
    return output;
}

[shader("fragment")]
float2 FragmentMain(VertexOutput input) : SV_Target
{
    float2 currentUV = (input.currentClip.xy / input.currentClip.w) * 0.5 + 0.5;
    float2 previousUV = (input.previousClip.xy / input.previousClip.w) * 0.5 + 0.5;
    return currentUV - previousUV;
}
