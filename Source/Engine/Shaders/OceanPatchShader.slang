import WeilanEngine;
import Library.PerScene;

// should match the declaration in OceanRenderer
struct GPUNodeInstanceData
{
    float2 position;
    float2 scale;
};

struct InputParams
{
    StructuredBuffer<GPUNodeInstanceData> instanceData;
};

struct Attributes
{
    float3 position : POSITION;
};

struct Varyings
{
    float4 position : SV_Position;
    float3 worldPos;
};

struct DrawParams
{
    int lod;
};

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<InputParams> params;

[vk_push_constant]
DrawParams drawParams;

[shader("vertex")]
Varyings Vertex(Attributes input, int instanceID :  SV_InstanceID, int baseInstance : SV_StartInstanceLocation)
{
    var instanceData = params.instanceData[baseInstance + instanceID];

    float3 positionWS = input.position;
    positionWS.xz *= instanceData.scale;
    positionWS.xz += instanceData.position;

    float4 positionCS = perScene.WorldToClipSpace(positionWS);

    Varyings output;
    output.position = positionCS;
    output.worldPos = positionWS;
    return output;
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
{
    return float4(frac(input.worldPos), 1.0);
}
