import Library.PerScene;

#if CONFIG
stencil:
    testEnable : True
    passOp: replace
    compareOp: always
    reference: 1
depth:
    testEnable: False
#endif

struct VertexOutput
{
    float4 position : SV_Position;
}

struct VertexInput
{
    float3 position;
}

ParameterBlock<PerScene> perScene;

struct Transform
{
    matrix<float, 4,4> model;
}

[vk::push_constant]
Transform transform;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(perScene.camera.viewProjection, mul(transform.model, float4(input.position, 1.0f)));
    return output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    return float4(1,1,1,1);
}

