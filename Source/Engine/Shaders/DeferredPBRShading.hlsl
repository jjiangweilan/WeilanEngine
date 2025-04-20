#pragma once

#define NearZero 0.000001
#define M_PI 3.1415926

float3 EncodeGBufferNormal(float3 normal)
{
    return normal * 0.5 + 0.5;
}

float3 DecodeGBufferNormal(float3 normal)
{
    return normal * 2 - 1;
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

float3 CalcIndirectSpecularFromCubemap(float3 v, float3 n, float metallic, SamplerCube specularCubemap, Sampler2D specularBRDFIntegrationMap, float roughness, float3 baseColor)
{
    float dotNV = clamp(dot(v, n), 0, 1);
    float3 f0 = baseColor * metallic;
    float lod             = GetMipLevelFromRoughness(roughness);
    float3 refl = reflect(-v, n);
    float3 prefilteredColor = specularCubemap.SampleLevel(refl, lod).xyz;
    float2 envBRDF          = specularBRDFIntegrationMap.SampleLevel(float2(dotNV, roughness), 0).xy;
    // float3 energyCompensation = 1.0 + f0 * (1.0 / envBRDF.x - 1.0);
    float3 indirectSpecular = prefilteredColor * (f0 * envBRDF.x + envBRDF.y);// * energyCompensation;
    return indirectSpecular;
}

float3 CalcDiffuseCubemap(float3 n, SamplerCube diffuseCubemap, float metallic, float3 baseColor, float ambientLightScale)
{
    float3 diffuseCubeColor = diffuseCubemap.Sample(n).xyz;
    float3 ambient = (1 - metallic) * baseColor * diffuseCubeColor * ambientLightScale;
    return ambient;
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

