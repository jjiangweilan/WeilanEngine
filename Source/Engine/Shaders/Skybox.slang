import Library.PerScene;
import Sky_Common;

#if CONFIG
cull: none
depth:
    testEnable: True
    writeEnable: False
    compOp: greaterOrEqual
#endif

struct VertexInput
{
    float3 position;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionOS;
}

ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput Vert(VertexInput input)
{
    VertexOutput output;
    float4x4 view = perScene.camera.view;
    view[0][3] = 0;
    view[1][3] = 0;
    view[2][3] = 0;
    view[3][3] = 1;
    float4x4 projection = perScene.camera.projection;
    output.position = mul(projection, mul(view, float4(input.position, 1.0)));
    output.positionOS = input.position.xyz;
    output.position.z = 0;

    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input) : SV_Target
{
    return float4(Sky(perScene, 0, input.positionOS.xyz), 0.0);
}
