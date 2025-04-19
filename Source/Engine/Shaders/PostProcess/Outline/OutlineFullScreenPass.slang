#if CONFIG
depth:
    testEnable: False
stencil:
    testEnable: True
    passOp: keep
    compareOp: notEqual
    reference: 1
blend:
    - srcAlpha oneMinusSrcAlpha
cull: none
#endif

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}

struct Transform
{
    float4x4 model;
}

struct Parameter
{
    Sampler2D mainTex;
}
ParameterBlock<Parameter> parameter;

[vk::push_constant]
Transform transform;

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID)
{
    float4 c_Positions[6] = 
    {
        { -1, -1, 0.5, 1 },
        { -1, 1, 0.5, 1 },
        { 1, 1, 0.5, 1 },

        { -1, -1, 0.5, 1 },
        { 1, 1, 0.5, 1 },
        { 1, -1, 0.5, 1 }
    };

    VertexOutput output;
    output.position = c_Positions[vertexID];
    output.uv = output.position.xy * 0.5 + 1;

    return output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input)
{
    int3 coord = int3(int2(input.position.xy), 0);

    float4 c00 = parameter.mainTex.Load(coord + int3(-1,-1, 0));
    float4 c01 = parameter.mainTex.Load(coord + int3(-1, 0, 0));
    float4 c02 = parameter.mainTex.Load(coord + int3(-1, 1, 0));
    float4 c10 = parameter.mainTex.Load(coord + int3(0, -1, 0));
    float4 c12 = parameter.mainTex.Load(coord + int3(0, 1, 0));
    float4 c20 = parameter.mainTex.Load(coord + int3(1, -1, 0));
    float4 c21 = parameter.mainTex.Load(coord + int3(1, 0, 0));
    float4 c22 = parameter.mainTex.Load(coord + int3(1, 1, 0));

    // x axis
    float4 xVal = c00 * -1 + c01 * -2 + c02 * -1 + c20 * 1 + c21 * 2 + c22 * 1;
    float4 yVal = c00 * -1 + c10 * -2 + c20 * -1 + c02 * 1 + c12 * 2 + c22 * 1;

    if (dot(xVal, xVal) != 0 || dot(yVal, yVal) != 0)
    {
        return float4(1, 0.647, 0, 1);
    }

    return 0;
}
