import Library.PerScene;

#if CONFIG
polygonMode : line
depth :
    testEnable : true

#endif

struct Transform
{
    matrix<float,4,4> model;
} ;

[vk::push_constant]
Transform transform;


struct VertexInput
{
    float3 position;
    float3 normal;
    float4 color;
    float2 uv;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color;
}

ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;
    float3 positionWS = float3(mul(transform.model, float4(input.position, 1)).xyz);
    output.position = mul(perScene.viewProjection, float4(positionWS, 1));
    // slightly push the line to the camera
    output.position.z -= 0.0001;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 FragmentMain() : SV_Target
{
    return float4(0.0f, 1.0f, 0.f, 1.0f);
}
