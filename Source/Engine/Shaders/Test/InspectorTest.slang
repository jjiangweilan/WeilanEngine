import WeilanEngine;

[MaterialUILayout]
struct MaterialInput
{
    [Group("Surface"), Color]
    float3 baseColor;

    [Group("Surface"), Range(0.0, 1.0), Tooltip("How rough the surface is")]
    float roughness;

    [Group("Surface"), Range(0.0, 1.0)]
    float metallic;

    [Group("Emission"), Color]
    float3 emissiveColor;

    [Group("Emission"), Range(0.0, 10.0)]
    float emissiveIntensity;
};

[Material]
ParameterBlock<MaterialInput> mat;

[Material]
Texture2D baseColorTex;
[Material]
SamplerState baseColorTexSampler;

static extern const bool _AlphaClip = false;
static extern const bool _Emission = false;

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    output.position = float4(input.position, 1.0);
    output.uv = input.uv;
    return output;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    float3 color = mat.baseColor;
    if (_Emission)
    {
        color += mat.emissiveColor * mat.emissiveIntensity;
    }
    return float4(color, 1.0);
}
