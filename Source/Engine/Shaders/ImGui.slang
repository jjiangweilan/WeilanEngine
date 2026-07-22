import WeilanEngine;
import ColorSpace;
#if CONFIG
blend:
- srcAlpha oneMinusSrcAlpha
blendOp:
- add
cull: none
interleaved: true
#endif

struct Vertex
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    [[format("rgba8")]]
    float4 color : COLOR0;
}

struct PerMaterial
{
    Texture2D texture;
    SamplerState sampler_linear;
    SamplerState sampler_linear_clamp;
}

[Material]
ParameterBlock<PerMaterial> perMaterial;

struct VOut
{
    float4 position : SV_Position;
    float4 color;
    float2 uv;
}

struct PushConstant
{
    float2 scale;
    float2 translate;
    float useClampSampler;
}
[vk::push_constant]
PushConstant p;

[shader("vertex")]
VOut VertexMain(Vertex input)
{
    VOut output;
    output.position = float4(input.position * p.scale + p.translate, 0, 1);
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 FragmentMain(VOut input) : SV_Target
{
    if (p.useClampSampler > 0.0f)
    {
        let color = SRGBToLinear(input.color) * perMaterial.texture.Sample(perMaterial.sampler_linear_clamp, input.uv);
        return color;
    }
    else
    {
        let color = SRGBToLinear(input.color) * perMaterial.texture.Sample(perMaterial.sampler_linear, input.uv);
        return color;
    }
}
