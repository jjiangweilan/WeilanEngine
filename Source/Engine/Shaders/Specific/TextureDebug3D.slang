import WeilanEngine;
struct PerMaterial
{
    float layer;
    float axis;
    float channel;

#if GPU_RESOURCE
    Sampler3D tex;
#endif
}

#include "Utils/VertexFullScreen.hlsl"

[Material]
ParameterBlock<PerMaterial> perMaterial;

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float4 output = 0;

    float3 uv;

    if (perMaterial.axis == 0)
    {
        uv = float3(perMaterial.layer, input.uv.x, input.uv.y);
    }
    else if (perMaterial.axis == 1)
    {
        uv = float3(input.uv.x, perMaterial.layer, input.uv.y);
    }
    else
        uv = float3(input.uv.x, input.uv.y, perMaterial.layer);
        

    output = perMaterial.tex.Sample(uv)[(int)clamp(perMaterial.channel,0,3)];

    return output;
}


