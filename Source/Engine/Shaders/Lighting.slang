static const float NearZero = 0.00000001;

import Library.PerScene;
import Shadows;
import Math;

struct GI
{
    float ao;
    SamplerCube specularCubemap;
    Sampler2D specularBRDFIntegrationMap;
};

struct RayTracing
{
    RaytracingAccelerationStructure sceneBVH;
    uint maxRayRecursionDepth;
};

struct ShadingObject
{
    float3 positionWS;
    float3 normalWS;
    float3 baseColor;
    float3 viewDir;
    float metallic;
    float roughness;
};

struct MainLightShadowData
{
    float shadowConstantBias;
    float shadowNormalBias;
    Texture2D shadowMap;
    SamplerComparisonState shadowSampler;
};

float Pow2(float x)
{
    return x * x;
}

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

// Real Shading in Unreal Engine 4
float PointLightInverseSquareFalloff(float distance, float lightRadius)
{
    float dis_light = distance / lightRadius;
    dis_light *= dis_light;
    dis_light *= dis_light;
    const float artisticControll = 1.0f;
    return Pow2(clamp(1 - dis_light, 0, 1)) / (distance * distance + artisticControll);
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

// Not Correct
float3 Fresnel(float3 albedo, float metallic, float dotVH)
{
    float3 F0 = float3(0.04); 
    F0      = lerp(F0, albedo, metallic);
    float attenuation = pow(2, (-5.55472 * dotVH - 6.98316) * dotVH);
    return F0 + (1 - F0) * attenuation;
}

float3 Fresnel_Schlick(float dotVH, float3 f0)
{
    const float f90 = 1.0; // most Observation of real world materials show that both dielectrics and conductors exhibit achromatic specular reflectance at grazing angles and that the Fresnel reflectance is 1.0 at 90° [filament pbr rendering]
    return (f0 + (f90-f0)*(pow(1.0 - max(0.0, dotVH), 5.0)));
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

float Pow2(float x, float y)
{
    return x * y;
}

float D_GGX(float NoH, float roughness) {
    float r2 = roughness * roughness;
    return r2 / (M_PI * Pow2(NoH * NoH * (r2 - 1) + 1));
}

float G_SmithGGX(float NoV, float NoL, float roughness) {
    float k = Pow2(roughness + 1) / 8;
    float Gv = NoV / ((NoV)*(1-k) + k);
    float Gl = NoL / ((NoL)*(1-k) + k);

    return Gv * Gl;
}

float3 SpecularBRDF(float3 albedo, float3 l, float3 v, float3 n, float metallic, float roughness, out float3 F)
{
    float3 h = normalize(l + v);
    float dotNH = clamp(dot(n, h), NearZero, 1);
    float dotNV = clamp(dot(n, v), NearZero, 1);
    float dotNL = clamp(dot(n, l), NearZero, 1);
    float dotVH = clamp(dot(v, h), NearZero, 1);

    float D = D_GGX(dotNH, roughness);

    // geometry
    float G = G_SmithGGX(dotNV, dotNL, roughness);

    // fresnel
    F = Fresnel_Schlick(dotVH, lerp(0.04, albedo, metallic));

    float3 brdf = G * D * F / (4 * dotNL * dotNV);
    return brdf;
}

struct PointLightShadowData
{
    TextureCube shadowMap;
    SamplerComparisonState shadowSampler;
    int   lightIndex;
    float farPlane;
    float depthBias;
    float3 lightPosition;
};

float3 StandardLightShading_DirectLightOnly(
        SamplerState linearClamp,
        SamplerState pointClamp,
        float2 screenUV,
        Light light,
        MainLightShadow mainLightShadow,
        ShadingObject object)
{
        float4 lightPos = light.position;
        float3 objPos = object.positionWS;
        float3 objNormal = object.normalWS;

        float3 l = lightPos.a == 0 ? lightPos.xyz : normalize(lightPos.xyz - objPos);

        float dotNL = max(dot(objNormal, l), NearZero);
        float3 F = 0.0f;
        float3 specularBrdf = SpecularBRDF(object.baseColor, l, object.viewDir, object.normalWS, object.metallic, object.roughness, F);

        float3 kd = 1 - F;
        kd *= 1 - object.metallic;

        float3 inputLight = dotNL * light.intensity * light.lightColor.xyz;

        // falloff
        float falloff = 1.0f;
        if (lightPos.a != 0) 
        {
            falloff = PointLightInverseSquareFalloff(distance(lightPos.xyz, object.positionWS.xyz),  light.pointLightTerm2);
        }

        float3 lighting = inputLight * (kd * object.baseColor / M_PI + specularBrdf) * falloff; // this looks wrong for metal, metal shouldn't diffuse too much

        float3 color = lighting;
        return color;
}

float3 StandardLightShading(
        SamplerState linearClamp,
        SamplerState pointClamp,
        float2 screenUV,
        Light light,
        Camera camera,
        MainLightShadow mainLightShadow,
        ShadingObject object,
        MainLightShadowData mainLightShadowData,
        GI gi,
        Texture2D contactShadowMap,
        int lightIndex,
        PointLightShadowData pointLightShadowData)
{
        float4 lightPos = light.position;
        float3 objPos = object.positionWS;
        float3 objNormal = object.normalWS;

        float3 l = lightPos.a == 0 ? lightPos.xyz : normalize(lightPos.xyz - objPos);

        float dotNL = max(dot(objNormal, l), NearZero);
        float3 F = 0.0f;
        float3 specularBrdf = SpecularBRDF(object.baseColor, l, object.viewDir, object.normalWS, object.metallic, object.roughness, F);

        float3 kd = 1 - F;
        kd *= 1 - object.metallic;

        float3 inputLight = dotNL * light.intensity * light.lightColor.xyz;

        // falloff
        float falloff = 1.0f;
        if (lightPos.a != 0) 
        {
            falloff = PointLightInverseSquareFalloff(distance(lightPos.xyz, object.positionWS.xyz),  light.pointLightTerm2);

            // point light shadow
            if (lightIndex == pointLightShadowData.lightIndex && pointLightShadowData.farPlane > 0)
            {
                falloff *= GetPointLightShadowAttenuation(
                    objPos, pointLightShadowData.lightPosition,
                    pointLightShadowData.farPlane, pointLightShadowData.depthBias,
                    pointLightShadowData.shadowMap, pointLightShadowData.shadowSampler);
            }
        }

        float3 lighting = inputLight * (kd * object.baseColor / M_PI * gi.ao + specularBrdf) * falloff;// this looks wrong for metal, metal shouldn't diffuse too much
        // possible solution https://bruop.github.io/ibl/
        // const float DIELECTRIC_SPECULAR = 0.04;
        // diffuseColor = albedo.rgb * (1 - DIELECTRIC_SPECULAR) * (1.0 - metalness)

        // shadow mask
        float mainLightShadowAttenuation = 1.0;
        if (lightPos.a == 0)
        {
            const float shadowBias = mainLightShadowData.shadowConstantBias;
            const float shadowNormalBias = mainLightShadowData.shadowNormalBias;

            mainLightShadowAttenuation = GetShadowAttenuation(objPos, objNormal, shadowBias, shadowNormalBias,
                camera,
                mainLightShadow,
                mainLightShadowData.shadowMap,
                mainLightShadowData.shadowSampler);
        }

        // Apply screen space shadow
        float contactShadow = contactShadowMap.SampleLevel(linearClamp, screenUV, 0.0).x;

        float shadowAttenuation = min(contactShadow, mainLightShadowAttenuation);

        float3 color = lighting * shadowAttenuation;
        return color;
}
