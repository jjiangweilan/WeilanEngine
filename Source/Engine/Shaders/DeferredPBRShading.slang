import Library.PerScene;

#if CONFIG
depth:
    testEnable: false
    writeEnable: false
#endif

#include "Utils/VertexFullScreen.hlsl"
#include "DeferredPBRShading.hlsl"
#include "DeferredPBRShadingInput.hlsl"
#include "Shadows.hlsl"

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
    SamplerComparisonState samplerShadow;
}

ParameterBlock<PerScene> perScene;
ParameterBlock<DeferredInput> perMaterial;

#define NearZero 0.000001
#define M_PI 3.1415926

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

float3 Fresnel(float3 albedo, float metallic, float dotVH)
{
    float3 F0 = float3(0.04); 
    F0      = lerp(F0, albedo, metallic);
    float attenuation = pow(2, (-5.55472 * dotVH - 6.98316) * dotVH);
    return F0 + (1 - F0) * attenuation;
}

// https://google.github.io/filament/Filament.html
// this handles half precision well
#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
float D_GGX_half(float roughness, float NoH, const float3 n, const float3 h) {
    float3 NxH = cross(n, h);
    float a = NoH * roughness;
    float k = roughness / (dot(NxH, NxH) + a * a);
    float d = k * k * (1.0 / M_PI);
    return saturateMediump(d);
}

// original
float D_GGX(float NoH, float roughness) {
    // float r2 = roughness * roughness;
    // r2 / (M_PI * pow2(pow2(dotNH) * (r2 - 1) + 1));
    float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / M_PI);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = roughness * roughness;
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

float3 SpecularBRDF(float3 albedo, float3 l, float3 v, float3 n, float metallic, float roughness, out float3 F)
{
    float3 h = normalize(l + v);
    float dotNH = clamp(dot(n, h), NearZero, 1);
    float dotNV = clamp(dot(n, v), NearZero, 1);
    float dotNL = clamp(dot(n, l), NearZero, 1);
    float dotVH = clamp(dot(v, h), NearZero, 1);

    float D = D_GGX_half(roughness, dotNH, n, h);

    // geometry
    float V = V_SmithGGXCorrelated(dotNV, dotNL, roughness);

    // fresnel
    F = Fresnel(albedo, metallic, dotVH);

    float3 brdf = V * D * F;
    return brdf;
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
float GetMipLevelFromRoughness(float roughness)
{
    roughness = max(roughness, 0.001);
    if (roughness <= 0.2)
    {
        return lerp(0, 1, (roughness - 0.001) / 0.2);
    }

    if (roughness <= 0.5)
    {
        return lerp(1, 2, (roughness - 0.2) / 0.3);
    }

    if (roughness <= 0.8)
    {
        return lerp(2, 3, (roughness - 0.5) / 0.3);
    }

    if (roughness <= 1)
    {
        return lerp(3, 4, (roughness - 0.8) / 0.2);
    }

    return 0;
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
        float3 F;
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
            if (shadowUV.x < 1 && shadowUV.y < 1 && shadowUV.x > 0 && shadowUV.y > 0)
            {
                shadowAttenuation = PcfShadow(shadowUV, perMaterial.input.shadowMapTexelSize.zw, objShadowDepth, perMaterial.shadowMap, perMaterial.samplerShadow);
            }
        }
        color += lighting * shadowAttenuation;
    }

    // specular cube light
    float dotNV = clamp(dot(v, n), 0, 1);
    float3 f0 = baseColor * metallic;
    float lod             = GetMipLevelFromRoughness(roughness);
    float3 reffloat = reflect(-v, n);
    float3 prefilteredColor = perMaterial.specularCube.SampleLevel(reffloat, lod).xyz;
    float2 envBRDF          = perMaterial.specularBRDFIntegrationMap.SampleLevel(float2(dotNV, roughness), 0).xy;
    // float3 energyCompensation = 1.0 + f0 * (1.0 / envBRDF.x - 1.0);
    float3 indirectSpecular = prefilteredColor * (f0 * envBRDF.x + envBRDF.y);// * energyCompensation;

    // diffuse cube light
    float ao = perMaterial.ambientOcclusion.SampleLevel(input.uv, 0).x;
    float3 diffuseCubeColor = perMaterial.diffuseCube.Sample(n).xyz;
    float3 ambient = (1 - metallic) * ao * baseColor * diffuseCubeColor * (perScene.lightCount.x > 0 ? perScene.lights[0].ambientScale : 1);

    return float4(color + ambient + indirectSpecular, 1);
}
