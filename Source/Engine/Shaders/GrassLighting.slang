import WeilanEngine;
import Shadows;
import Lighting;
import Library.PerScene;

#include "DeferredPBRShadingInput.hlsl"

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
    DeferredPBRShadingInput input;
    Sampler2D albedoTex;
    Sampler2D normalTex;
    Sampler2D depthTex;
    Sampler2D ambientOcclusion;
    Texture2D shadowMap;
    SamplerComparisonState samplerShadow_clamp;
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
    float3 normalWS = normalize(DecodeGBufferNormal(perMaterial.normalTex.Sample(input.uv).xyz));
    float depth = perMaterial.depthTex.Sample(input.uv).x;
    float4 positionWS = mul(perScene.camera.invNDCToWorld, float4(input.uv * 2 - 1, depth, 1.0));
    positionWS /= positionWS.w;

    float shadowAttenuation = GetShadowAttenuation(
        positionWS.xyz,
        normalWS,
        perMaterial.input.shadowConstantBias,
        perMaterial.input.shadowNormalBias,
        perScene.camera,
        perScene.mainLightShadow,
        perMaterial.shadowMap,
        perMaterial.samplerShadow_clamp);

    float ao = perMaterial.ambientOcclusion.SampleLevel(input.uv, 0).x;
    return float4(albedo * ao * shadowAttenuation, 1.0);
}
