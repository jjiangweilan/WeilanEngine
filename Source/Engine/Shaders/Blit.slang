#include "Utils/VertexFullScreen.hlsl"

static extern const bool _ResetAlpha = false;

struct PerMaterial
{
    Sampler2D input;
};

ParameterBlock<PerMaterial> perMaterial;

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
if (_ResetAlpha)
{
    return float4(perMaterial.input.Sample(input.uv).xyz, 1.0);
}
else
{
    return perMaterial.input.Sample(input.uv);
}
}
