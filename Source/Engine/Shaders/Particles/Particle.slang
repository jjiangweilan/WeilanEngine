import WeilanEngine;
import Library.PerScene;
#include "Particle.hlsl"

#if CONFIG
name : Particle
depth:
    testEnable: true
    writeEnable: false
blend:
    - srcAlpha oneMinusSrcAlpha
#endif

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_Position;

    nointerpolation
    uint instanceID : TEXCOORD0;
};

struct ParticleParameter
{
    float particleCount;
    float4x4 facingCameraRotation;
    StructuredBuffer<GPUParticle> worldMatrices;

    float GetParticleCount()
    {
        return particleCount;
    }

    float4x4 GetParticleWorldMatrix(int index)
    {
        return worldMatrices[index].worldMatrix;
    }

    float4 GetColor(int idx)
    {
        return worldMatrices[idx].color;
    }
};

struct ModelTransform
{
    float4x4 model;
}
[vk_push_constant]
ModelTransform pushConstant;

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<ParticleParameter> param;
[shader("vertex")]
VertexOutput Vert(VertexInput input, uint instanceID : SV_InstanceID)
{
    VertexOutput output;
    output.position = perScene.WorldToClipSpace(mul(param.GetParticleWorldMatrix(instanceID), float4(input.position,1.0)));
    output.instanceID = instanceID;
    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input) : SV_Target
{
    return param.GetColor(input.instanceID);
}


