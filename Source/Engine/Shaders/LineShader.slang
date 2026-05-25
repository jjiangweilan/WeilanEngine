import WeilanEngine;
import Library.PerScene;

#if CONFIG
topology : lineList
cull : off
#endif

struct LineData
{
    float4 fromWS;
    float4 toWS;
    float4 color;
};

[Global]
ParameterBlock<PerScene> perScene;

struct LineInput
{
    StructuredBuffer<LineData> lineData;
};

[Material]
ParameterBlock<LineInput> lineInput;

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
}

[shader("vertex")]
VertexOutput VertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    uint lineIndex = vertexId / 2;
    LineData line = lineInput.lineData[lineIndex];
    float4 pos = float4((vertexId % 2 == 0 ? line.fromWS : line.toWS).xyz, 1.0f);
    output.position = mul(perScene.camera.viewProjection, pos);
    output.color = line.color;

    return output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    return input.color;
}
