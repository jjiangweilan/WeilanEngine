import Library.PerScene;
import WeilanEngine;
#if CONFIG
name: Billboard
depth:
    testEnable: false
#endif

struct Transform
{
    float4 position;
    float4 scale;
};

struct FragInput
{
    Sampler2D mainTex;
}

[vk::push_constant]
Transform transform;

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<FragInput> perMaterial;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID)
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
    output.uv = c_Positions[vertexID].xy * 0.5f + 0.5f;
    output.uv.y = 1 - output.uv.y;

    float4 vPos = c_Positions[vertexID];
    vPos.xy *= transform.scale.xy;
    float3 lookat = perScene.viewPos.xyz - transform.position.xyz;
    float3 anyvec = all(lookat != float3(0, 1, 0)) ? float3(0, 1, 0) : float3(1, 1, 0);
    float3 right = cross(lookat, anyvec);
    float3 up = cross(right, lookat);

    float4x4 billboardModelMatrix = {
        float4(normalize(right.xyz), 0),
        float4(0,1,0, 0),
        float4(normalize(lookat.xyz), 0),
        float4(transform.position.xyz, 1)
        };
    billboardModelMatrix = transpose(billboardModelMatrix);
    output.position = mul(perScene.viewProjection, mul(billboardModelMatrix, vPos));

    return output;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    float4 oColor = perMaterial.mainTex.Sample(input.uv);
    if (oColor.a < 0.5)
        discard;

    oColor.a = 0.6;

    return oColor;
}
