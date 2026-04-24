import Library.PerScene;

#if CONFIG
topology : lineList
cull : off
#endif

struct PushConstant
{
    float4 fromWS;
    float4 toWS;
    float4 color;
};

[vk::push_constant]
PushConstant p;

ParameterBlock<PerScene> perScene;

struct VertexOutput
{
    float4 position : SV_Position;
}

[shader("vertex")]
VertexOutput VertexMain(uint vertexId : SV_VertexID)
{
    VertexOutput output;
    float4 pos = float4(vertexId == 0 ? p.fromWS.xyz : p.toWS.xyz, 1.0f);
    output.position = mul(perScene.camera.viewProjection, pos);

    return output;
}

[shader("fragment")]
float4 FragmentMain() : SV_Target
{
    return p.color;
}
