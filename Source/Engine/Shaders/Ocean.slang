#if CONFIG
name: Ocean
#endif

import WeilanEngine;
import Library.PerScene;

struct VertexOutput
{
    float4 position : SV_Position;
};

struct VertexInput
{
    float3 position : POSITION;
};

ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    return output;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{

    return float4(1.0, 1.0, 1.0, 1.0);
}


