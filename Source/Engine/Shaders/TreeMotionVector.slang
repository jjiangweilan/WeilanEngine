import WeilanEngine;
import Library.PerScene;
import ObjectWorldPosition;
import TreeWind;

static extern const bool _ObjectMotion = false;

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
    GpuMaterial material = entity.GetMaterial(perScene);

    bool hasBoneWeight = false;
    float4x4 currentModelMatrix = entity.GetModelMatrix();
    float4x4 currentObjectModelMatrix = GetObjectModelMatrix(
        perScene,
        currentModelMatrix,
        entity.skeletonOffset,
        entity.GetBone(),
        hasBoneWeight
    );
    float3 currentWorld = ApplyTreeWind(
        entity.GetPosition(),
        currentObjectModelMatrix,
        material,
        perScene.scene.time
    );

    float4x4 previousObjectModelMatrix = currentObjectModelMatrix;
    if (_ObjectMotion)
    {
        GPUDynamicMotionData motionData = perScene.LoadDynamicData<GPUDynamicMotionData>(
            pconst.firstDynamicMotionDataByteOffset,
            drawIndex
        );
        previousObjectModelMatrix = GetObjectModelMatrix(
            perScene,
            motionData.previousWorldMatrix,
            motionData.previousSkeletonOffset,
            entity.GetBone(),
            hasBoneWeight
        );
    }
    float3 previousWorld = ApplyTreeWind(
        entity.GetPosition(),
        previousObjectModelMatrix,
        material,
        perScene.scene.previousTime
    );

    VertexOutput output;
    output.currentClip = mul(perScene.camera.viewProjection, float4(currentWorld, 1.0f));
    output.previousClip = mul(perScene.camera.previousViewProjection, float4(previousWorld, 1.0f));
    output.position = output.currentClip;
    return output;
}

[shader("fragment")]
float2 FragmentMain(VertexOutput input) : SV_Target
{
    float2 currentUV = (input.currentClip.xy / input.currentClip.w) * 0.5f + 0.5f;
    float2 previousUV = (input.previousClip.xy / input.previousClip.w) * 0.5f + 0.5f;
    float2 jitterDelta = perScene.camera.temporalJitter.xy - perScene.camera.temporalJitter.zw;
    return currentUV - previousUV - jitterDelta;
}
