import WeilanEngine;
#if CONFIG
blend:
- one oneMinusSrcAlpha one oneMinusSrcAlpha
blendOp:
- add
cull: none
interleaved: true
depth:
    test: false
    write: false
#endif

struct Vertex
{
    float2 position : POSITION;
    [[format("rgba8")]]
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
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
    float4x4 transform;
    float2 scale;
    float2 translate;
    float2 geometryTranslate;
    float useTexture;
}

[vk::push_constant]
PushConstant p;

[shader("vertex")]
VOut VertexMain(Vertex input)
{
    VOut output;

    float4 pixelPosition = mul(p.transform, float4(input.position + p.geometryTranslate, 0.0f, 1.0f));
    output.position = float4(pixelPosition.x * p.scale.x + p.translate.x, pixelPosition.y * p.scale.y + p.translate.y, pixelPosition.z, pixelPosition.w);
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 FragmentMain(VOut input) : SV_Target
{
    float4 color = input.color;
    if (p.useTexture > 0.0f)
    {
        color *= perMaterial.texture.Sample(perMaterial.sampler_linear_clamp, input.uv);
    }

    return color;
}
