import Library.PerScene;

struct GrassClusterVSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct GrassClusterVarings
{
    float4 position : SV_POSITION;
    float3 normal  : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct GrassClusterPassInput
{
    StructuredBuffer<float4> worldPositions;
}

ParameterBlock<PerScene> scene;
ParameterBlock<GrassClusterPassInput> passInput;

[[shader("vertex")]]
GrassClusterVarings VSMain(GrassClusterVSInput input, uint instanceID : SV_InstanceID)
{
    GrassClusterVarings output;

    float3 worldPosOffset = passInput.worldPositions[instanceID].xyz;
    float3 worldPosition = input.position + worldPosOffset;

    output.position = scene.WorldToClipSpace(float4(worldPosition, 1.0));
    output.normal = scene.WorldToClipSpaceDir(input.normal);
    output.uv = input.uv;
    return output;
}

[[shader("fragment")]]
float4 FSMain(GrassClusterVarings input) : SV_TARGET
{
    return input.position / 100;
}
