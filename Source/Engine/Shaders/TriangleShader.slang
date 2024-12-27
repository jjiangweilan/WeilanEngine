#if CONFIG
topology : lineList
cull : off
#endif

import Library.PerScene;

struct PushConstnat
{
    float4 v0;
    float4 v1;
    float4 v2;
    float4 color;
};

[vk::push_constant]
PushConstnat p;

ParameterBlock<PerScene> scene;

struct VertexOutput
{
    float4 positon : SV_Position;
}

[shader("vertex")]
void VertexMain(uint vertexId : SV_VertexID)
{
    float4 v;
    if (vertexId == 0) v= p.v0;
    else if (vertexId == 1) v= p.v1;
    else if (vertexId == 2) v= p.v2;

    float4 pos = float4(v.xyz, 1.0f);

    VertexOutput output;
    output.positon= mul(scene.viewProjection, pos);
}

[shader("fragment")]
float4 FragmentMain() : SV_Target
{
    return p.color;
}
