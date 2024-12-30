import Library.PerScene;

#if CONFIG
cull: back
#endif

struct Transform
{
    matrix<float, 4,4> model;
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

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    float3 positionWS = float3(mul(transform.model, float4(input.position, 1.0)).xyz);

    VertexOutput output;
    output.position = mul(perScene.worldToShadow, float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}
