import WeilanEngine;
import Library.PerScene;

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: notEqual
stencil:
    testEnable: true
    writeMask: 0
    reference: 2
    compareOp: equal
    compareMask: 0xFF
#endif

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}

[Global]
ParameterBlock<PerScene> perScene;

struct GrassLightingInput
{
    Sampler2D albedoTex;
}

[Material]
ParameterBlock<GrassLightingInput> perMaterial;

[shader("vertex")]
VertexOutput VertexMain(uint id : SV_VertexID)
{
    float4 c_Positions[6] =
    {
        { -1, -1, 0.5, 1 },
        { -1, 1, 0.5, 1 },
        { 1, 1, 0.5, 1 },

        { -1, -1, 0.5, 1 },
        { 1, 1, 0.5, 1 },
        { 1, -1, 0.5, 1 }
    };

    VertexOutput output;
    output.position = c_Positions[id];
    output.uv = output.position.xy * 0.5 + 0.5;
    output.position.z = 1;

    return output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float3 albedo = perMaterial.albedoTex.Sample(input.uv).xyz;
    return float4(albedo, 1.0);
}