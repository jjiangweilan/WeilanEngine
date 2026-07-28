import WeilanEngine;
import Shadows;
import Library.PerScene;
import Math;

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: notEqual
stencil:
    testEnable: true
    writeMask: 0
    reference: 1
    compareOp: equal
    compareMask: 0xFF
#endif

import Lighting;
#include "DeferredPBRShadingInput.hlsl"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}

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

struct DeferredInput
{
    DeferredPBRShadingInput input;

    Sampler2D albedoTex;
    Sampler2D normalTex;
    Sampler2D maskTex;
    Sampler2D depthTex;
    Texture2D shadowMap;
    Texture2D contactShadowMap;
    SamplerCube diffuseCube;
    SamplerCube specularCube;
    Sampler2D specularBRDFIntegrationMap;
    Sampler2D ambientOcclusion;
    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;
    SamplerComparisonState samplerShadow_clamp;
    SamplerComparisonState samplerShadow;
    TextureCube pointLightShadowMap;
}

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<DeferredInput> perMaterial;

float2 SphericalToUV(float3 v)
{
    const float2 invAtan = float2(0.1591, 0.3183);
    // -asin because we have top-left uv origin
    float2 uv = float2(atan(v.z / v.x), -asin(v.y));

    // convert to [0,1]
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

// http://www.ogre3d.org/tikiwiki/tiki-index.php?page=-Point+Light+Attenuation
float PointLightAttenuation(float distance, float t1, float t2)
{
    float s = distance / t2;
    if (s < 1)
    {
        s *= s;
        float n0 = (1 - s);
        return n0 * n0 / (1 + t1 * s);
    }
    else
        return 0;
}

#define OCCLUDER_SEARCH_ITS 100
float3 ParallaxShadowReprojection(float3 P1, float3 L1, float3 L0)
{ 
    float k = 1.0 + length(L1 - L0); 
    float3 D = k * L1 - L0; 
 
    float3 S = P1; 
    float3 dS = D * 0.25; 
    float sum = 0, cnt = 0; 
    for (int i = 0; i < 8; ++i) 
    { 
        float4 Sp = mul(perScene.mainLightShadow.worldToShadow[0], float4(S, 1));
        Sp = Sp / Sp.w;
        float2 shadowUV = Sp.xy * 0.5 + 0.5;
        float occDepth = perMaterial.shadowMap.SampleLevel(perMaterial.sampler_point_clamp, shadowUV, 0).r; 
        if (occDepth < Sp.z) 
        { 
            sum += (Sp.z - occDepth) * 1000.0f;  // shadow map is a depth texture from 0 to 1, we scale it with length of near and far plane
            cnt += 1; 
        } 
        S += dS; 
    } 
 
    float d0 = cnt > 0 ? sum * (1 / cnt) : 0; 
    float3 P0 = P1 + d0 * D; 

    return P0;
} 

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float3 n = normalize(DecodeGBufferNormal(perMaterial.normalTex.Sample(input.uv).xyz));
    float4 mask = perMaterial.maskTex.Sample(input.uv);
    float depth = perMaterial.depthTex.Sample(input.uv).x;
    float4 positionWS = mul(perScene.camera.invNDCToWorld, float4(input.uv * 2 - 1, depth, 1.0));
    positionWS /= positionWS.w;
    float3 viewDir = normalize(perScene.camera.position.xyz - positionWS.xyz);

    ShadingObject shadingObject;
    shadingObject.positionWS = positionWS.xyz;
    shadingObject.normalWS = n;
    shadingObject.baseColor = perMaterial.albedoTex.Sample(input.uv).xyz;
    shadingObject.viewDir = viewDir;
    shadingObject.metallic = mask.x;
    shadingObject.roughness = mask.y;
    shadingObject.shadowIntensityScale = mask.z;

    MainLightShadowData mainLightShadowData;
    mainLightShadowData.shadowConstantBias = perMaterial.input.shadowConstantBias;
    mainLightShadowData.shadowNormalBias = perMaterial.input.shadowNormalBias;
    mainLightShadowData.shadowMap = perMaterial.shadowMap;
    mainLightShadowData.shadowSampler = perMaterial.samplerShadow_clamp;

    GI gi;
    gi.ao = perMaterial.ambientOcclusion.SampleLevel(input.uv, 0).x;
    gi.specularCubemap = perMaterial.specularCube;
    gi.specularBRDFIntegrationMap = perMaterial.specularBRDFIntegrationMap;

    // Add direct lightings
    float3 color = float3(0, 0, 0);

    PointLightShadowData pointLightShadowData;
    pointLightShadowData.shadowMap = perMaterial.pointLightShadowMap;
    pointLightShadowData.shadowSampler = perMaterial.samplerShadow;
    pointLightShadowData.lightIndex = perMaterial.input.pointLightShadowLightIndex;
    pointLightShadowData.farPlane = perMaterial.input.pointLightShadowFarPlane;
    pointLightShadowData.depthBias = perMaterial.input.pointLightShadowLightPosAndBias.w;
    pointLightShadowData.lightPosition = perMaterial.input.pointLightShadowLightPosAndBias.xyz;

    for (int i = 0; i < perScene.scene.lightCount; ++i)
    {
        color += StandardLightShading(
        perMaterial.sampler_linear_clamp,
        perMaterial.sampler_point_clamp,
        input.uv,
        perScene.scene.lights[i],
        perScene.camera,
        perScene.mainLightShadow, shadingObject, mainLightShadowData, gi, perMaterial.contactShadowMap,
        i,
        pointLightShadowData);
    }

    // IBL
    // float3 indirectSpecular = CalcIndirectSpecularFromCubemap(shadingObject.viewDir, shadingObject.normalWS, shadingObject.metallic, gi.specularCubemap, gi.specularBRDFIntegrationMap, shadingObject.roughness, shadingObject.baseColor);

    // use the highest mip for the indirect diffuse (this is wrong, the last mip only contains a single color)
    // float3 indirectDiffuse = (1 - shadingObject.metallic) * shadingObject.baseColor * gi.specularCubemap.SampleLevel(shadingObject.normalWS, 100).xyz * perScene.GetMainLight().ambientScale * gi.ao;

    // color = color + indirectDiffuse + indirectSpecular;

    return float4(color, 1);
}
