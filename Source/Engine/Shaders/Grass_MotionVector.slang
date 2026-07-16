import WeilanEngine;
import Library.PerScene;

#include "Grass_Common.slang"

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: equal
stencil:
    testEnable: true
    writeMask: 0
    reference: 2
    compareOp: equal
    compareMask: 0xFF
#endif

[Global]
ParameterBlock<PerScene> perScene;

[Object]
ParameterBlock<GrassParams> params;

[vk_push_constant]
GrassPushConstant pconst;

struct Varyings
{
    float4 position : SV_Position;
    float4 currentClip;
    float4 previousClip;
};

[shader("vertex")]
Varyings Vertex(GrassAttributes input, uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation)
{
    GrassPatchInstanceData instanceData = params.instanceData[baseInstance + instanceID];

    float3 currentWorld = GetGrassWorldPosition(input, instanceData, params, pconst, perScene.scene.time);
    float3 previousWorld = GetGrassWorldPosition(input, instanceData, params, pconst, perScene.scene.previousTime);

    Varyings output;
    output.currentClip = mul(perScene.camera.viewProjection, float4(currentWorld, 1.0));
    output.previousClip = mul(perScene.camera.previousViewProjection, float4(previousWorld, 1.0));
    output.position = output.currentClip;
    return output;
}

[shader("fragment")]
float2 Fragment(Varyings input) : SV_Target
{
    float2 currentUV = (input.currentClip.xy / input.currentClip.w) * 0.5 + 0.5;
    float2 previousUV = (input.previousClip.xy / input.previousClip.w) * 0.5 + 0.5;
    float2 jitterDelta = perScene.camera.temporalJitter.xy - perScene.camera.temporalJitter.zw;
    return currentUV - previousUV - jitterDelta;
}
