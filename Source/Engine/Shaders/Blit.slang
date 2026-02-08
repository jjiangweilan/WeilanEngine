#include "Utils/VertexFullScreen.hlsl"
import WeilanEngine;

static extern const bool _ResetAlpha = false;

struct PerMaterial
{
    Sampler2D input;
};

[Material]
ParameterBlock<PerMaterial> perMaterial;

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float4 color = perMaterial.input.Sample(input.uv);

    if (_ResetAlpha)
    {
        return float4(color.xyz, 1.0);
    }
    else
    {
        return color;
    }
}
