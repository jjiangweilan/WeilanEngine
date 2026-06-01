import WeilanEngine;
import Library.PerScene;
import ObjectWorldPosition;

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: equal
#endif

struct PushConstant
{
    uint firstGpuObjectOffset;
    uint firstDynamicMotionDataByteOffset;
};

[vk_push_constant]
PushConstant pconst;

[Global]
ParameterBlock<PerScene> perScene;

struct GPUDynamicMotionData
{
    float4x4 previousWorldMatrix;
    uint previousSkeletonOffset;
    uint padding0;
    uint padding1;
    uint padding2;
};

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
    GPUDynamicMotionData motionData = perScene.LoadDynamicData<GPUDynamicMotionData>(pconst.firstDynamicMotionDataByteOffset, drawIndex);

    bool hasBoneWeight = false;
    float4x4 modelMatrix = entity.GetModelMatrix();
    float4x4 objectModelMatrix = GetObjectModelMatrix(perScene, modelMatrix, entity.skeletonOffset, entity.GetBone(), hasBoneWeight);

    float3 currentWorld = mul(objectModelMatrix, float4(entity.GetPosition(), 1.0)).xyz;

    float3 previousWorld  = mul(GetObjectModelMatrix(perScene, motionData.previousWorldMatrix, motionData.previousSkeletonOffset, entity.GetBone(), hasBoneWeight), float4(entity.GetPosition(), 1.0)).xyz;

    VertexOutput output;
    output.currentClip = mul(perScene.camera.viewProjection, float4(currentWorld, 1.0));
    output.previousClip = mul(perScene.camera.previousViewProjection, float4(previousWorld, 1.0));
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
