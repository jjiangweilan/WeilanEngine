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

ParameterBlock<PerScene> scene;

struct VertexOutput
{
    float4 position : SV_Position;
}

[shader("vertex")]
void VertexMain(uint vertexId : SV_VertexID)
{
VertexOutput output;
    float4 pos = float4(vertexId == 0 ? p.fromWS.xyz : p.toWS.xyz, 1.0f);
    output.position = mul(scene.viewProjection, pos);
}

[shader("fragment")]
float4 FragmentMain() : SV_Target
{
    return p.color;
}
