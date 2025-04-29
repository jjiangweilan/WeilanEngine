import Library.PerScene;
import WeilanEngine;

#if CONFIG
depth:
    testEnable: true
    writeEnable: false
    compOp: notEqual
#endif

#include "DeferredPBRShading.hlsl"
#include "DeferredPBRShadingInput.hlsl"
#include "Shadows.hlsl"

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
    SamplerCube diffuseCube;
    SamplerCube specularCube;
    Sampler2D specularBRDFIntegrationMap;
    Sampler2D ambientOcclusion;
    SamplerState sampler_point_clamp;
    SamplerComparisonState samplerShadow_clamp;
}

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<DeferredInput> perMaterial;

float pow2(float x)
{
    return x * x;
}

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

// Real Shading in Unreal Engine 4
float PointLightInverseSquareFalloff(float distance, float lightRadius)
{
    float dis_light = distance / lightRadius;
    dis_light *= dis_light;
    dis_light *= dis_light;
    const float artisticControll = 1.0f;
    return pow2(clamp(1 - dis_light, 0, 1)) / (distance * distance + artisticControll);
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
        float4 Sp = mul(perScene.worldToShadow, float4(S, 1));
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
    float metallic = mask.x;
    float roughness = mask.y;
    float3 baseColor = perMaterial.albedoTex.Sample(input.uv).xyz;
    float depth = perMaterial.depthTex.Sample(input.uv).x;
    float4 positionWS = mul(perScene.invNDCToWorld, float4(input.uv * 2 - 1, depth, 1.0));
    positionWS /= positionWS.w;
    float3 v = normalize(perScene.viewPos.xyz - positionWS.xyz);

    float3 color = float3(0, 0, 0);
    for (int i = 0; i < perScene.lightCount.x; ++i)
    {
        float4 lightPos = perScene.lights[i].position;
        float3 l = lightPos.a == 0 ? lightPos.xyz : normalize(lightPos.xyz - positionWS.xyz);

        float dotNL = max(dot(n, l), NearZero);
        float3 F = 0.0f;
        float3 brdf = SpecularBRDF(baseColor, l, v, n, metallic, roughness, F);
        float3 kd = 1 - F;
        kd *= 1 - metallic;

        float3 light = dotNL * perScene.lights[i].intensity * perScene.lights[i].lightColor.xyz;
        float attenuation = lightPos.a == 0 ? 1 : PointLightInverseSquareFalloff(distance(lightPos.xyz, positionWS.xyz),  perScene.lights[i].pointLightTerm2);
// global.perScene.lights[i].pointLightTerm1,;

        float3 lighting = light * (kd * baseColor / M_PI + brdf) * attenuation; // this looks wrong for metal, metal shouldn't diffuse too much
        // possible solution https://bruop.github.io/ibl/
        // const float DIELECTRIC_SPECULAR = 0.04;
        // diffuseColor = albedo.rgb * (1 - DIELECTRIC_SPECULAR) * (1.0 - metalness)

        // shadow mask
        float shadowAttenuation = 1.0;
        if (lightPos.a == 0)
        {
            const float shadowBias = perMaterial.input.shadowConstantBias;
            const float shadowNormalBias = perMaterial.input.shadowNormalBias;

            float3 shadowWorldPos = positionWS.xyz;
            if (perScene.cachedMainLightDirection.a == 1.0f)
            {
                shadowWorldPos = ParallaxShadowReprojection(positionWS.xyz, l, perScene.cachedMainLightDirection.xyz);
            }

            float4 shadowCS = mul(perScene.worldToShadow, float4(shadowWorldPos.xyz + shadowNormalBias * n, 1.0));
            shadowCS = shadowCS / shadowCS.w;
            float2 shadowUV = shadowCS.xy * 0.5 + 0.5;
            float objShadowDepth = shadowCS.z - shadowBias;
            bool valid = all(float3(shadowUV, shadowCS.z) > 0) && all(float3(shadowUV, shadowCS.z) < 1);
            if (valid)
                shadowAttenuation = PcfShadow(shadowUV, perMaterial.input.shadowMapTexelSize.zw, objShadowDepth, perMaterial.shadowMap, perMaterial.samplerShadow_clamp);
        }
        color += lighting * shadowAttenuation;
    }

    float ao = perMaterial.ambientOcclusion.SampleLevel(input.uv, 0).x;
    float3 ambient = perScene.GetMainLight().ambientScale * baseColor * (1 - metallic);
    color += ambient;

    return float4(color, 1);
}
