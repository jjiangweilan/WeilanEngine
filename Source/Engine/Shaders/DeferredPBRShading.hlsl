#pragma once

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
