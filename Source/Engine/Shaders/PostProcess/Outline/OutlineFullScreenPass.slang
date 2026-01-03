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
float4 FragmentMain(VertexOutput input) : SV_Target
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
    float3 xVal = c00.xyz * -1 + c01.xyz * -2 + c02.xyz * -1 + c20.xyz * 1 + c21.xyz * 2 + c22.xyz * 1;
    float3 yVal = c00.xyz * -1 + c10.xyz * -2 + c20.xyz * -1 + c02.xyz * 1 + c12.xyz * 2 + c22.xyz * 1;

    float4 c11 = parameter.mainTex.Load(coord);

    if (dot(xVal, xVal) != 0 || dot(yVal, yVal) != 0)
    {
        int selected = 
        (int)((c00.x * c00.a + c01.x * c01.a + c02.x * c02.a +
        c10.x * c10.a + c11.x * c11.a + c12.x * c12.a +
        c20.x * c20.a + c21.x * c21.a + c22.x * c22.a) / (c00.x + c01.x + c02.x + c10.x + c11.x + c12.x + c20.x + c21.x + c22.x));

        if (selected == 0)
            return float4(1, 0.647, 0, 1);
        else
            return float4(0.85, 0.863, 0.834, 1);
    }

    return 0;
}
