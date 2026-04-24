import WeilanEngine;
import Library.PerScene;
import Library.RayTracing;
import Math;

#if CONFIG
depth:
    testEnable: false
    writeEnable: false
stencil:
    testEnable: false
#endif

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}
// test
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

struct RayTracingInput
{
    Texture2D depthTex;
    SamplerState sampler_linear_clamp;
    RaytracingAccelerationStructure sceneBVH;
}

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<RayTracingInput> perMaterial;

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float depth = perMaterial.depthTex.SampleLevel(perMaterial.sampler_linear_clamp, input.uv, 0).x;
    if (depth == 0.0) return float4(1, 1, 1, 1); // background is lit

    float4 positionNDC = float4(input.uv * 2 - 1, depth, 1.0);
    float4 positionWS = mul(perScene.camera.invNDCToWorld, positionNDC);
    positionWS /= positionWS.w;

    float3 lightDir = normalize(perScene.scene.lights[0].position.xyz); // Assume light 0 is directional
    if (perScene.scene.lights[0].position.w != 0)
    {
        lightDir = normalize(perScene.scene.lights[0].position.xyz - positionWS.xyz);
    }

    float3 rayOrigin = positionWS.xyz + lightDir * 0.01;
    if (TraceVisibilityRay(perMaterial.sceneBVH, rayOrigin, lightDir))
    {
        return float4(0, 0, 0, 1); // Shadow
    }

    return float4(1, 1, 1, 1); // Lit
}
