import Library.PerScene;

#if CONFIG
name: Game/Cloud
cull: back
blend:
- one srcAlpha
#endif

struct VolumetricCloudParameter {
    float4 cloudPlane;
    float4 stratusType;
    float4 stratocumulusType;
    float4 cumulusType;
    float4 areaMapRemap;
    float4 planet;
    float4 cloudAreaMap_TexelSize;
    float2 ambientHeightScale;
    float cloudHeightRange;
    float maxThickness; // max thickness in normalized scale
    float fixedStep;    // fixed step in normalized scale
    float extinction;
    float lightExtinction;
    float lightPhase_g;
    float lightPhase_K;
    float areaMapPixelWorldSize; //sampleScale.z
    float detailMapScale; // sampleScale.x
    float cloudSphereBend; // sampleScale.y
    float nearPlane;
    float radiusTest;
    float lightScale;
    float lightStep;
    float lightStepCount;

#if GPU_RESOURCE
    [format("rgba8")]
    RWTexture2D<float4> mainColor;
    Texture3D cloudDensity;
    Texture2D cloudAreaMap;
    Texture2D depthMap;
    SamplerState sampler_Linear_Repeat;
    SamplerState sampler_Linear_Clamp;
#endif
};

ParameterBlock<PerScene> perScene;
ParameterBlock<VolumetricCloudParameter> perMaterial;


float remap(float value, float originMin, float originMax, float newMin,
        float newMax) {
    return newMin +
        (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

float CloudType_Get(float y, float4 type) {
    return saturate(remap(y, type.x, type.y, 0, 1)) *
        saturate(remap(y, type.z, type.w, 1, 0));
}

float Transmittance(float d) { return exp(-1 * d) ; }

float HenyeyGreenstein(float g, float costh) {
    return (1.0 - g * g) /
        (4.0 * 3.1415926 * pow(1.0 + g * g - 2.0 * g * costh, 3.0 / 2.0));
}

float DualLobHenyeyGreenstein(float g, float costh, float K) {
    return lerp(HenyeyGreenstein(-g, costh), HenyeyGreenstein(g, costh), K);
}

struct CloudInfo
{
    float density;
    float dimensionalProfile;
    float normalizedHeight;
};

float SdfAreaMap(float3 worldPos)
{
    return saturate(smoothstep(1, 0.8, saturate(length(worldPos) / perMaterial.radiusTest)));
}

CloudInfo SampleDensity(float3 worldPos)
{
    CloudInfo info;
    float areaMap = SdfAreaMap(worldPos);
    info.density = areaMap;
    info.dimensionalProfile = areaMap;
    info.normalizedHeight = 1.0f;

    float3 detailSamplePos = (worldPos) / perMaterial.detailMapScale;
    float4 density = perMaterial.cloudDensity.SampleLevel(perMaterial.sampler_Linear_Repeat, detailSamplePos.xzy, 0);
    float lowFrequencyFbm = 0.625 * density.y + 0.25 * density.z + 0.125 * density.w;

    info.density = saturate(lowFrequencyFbm - (1 - info.dimensionalProfile));
    return info;

    const float4 cloudType = perMaterial.stratusType;
    float cameraDistance = length(worldPos - perScene.viewPos.xyz);
    float cloudHeight =  max(0, worldPos.y - perMaterial.cloudPlane.w) / perMaterial.cloudHeightRange;
    cloudHeight += cameraDistance / (perMaterial.cloudSphereBend * perMaterial.cloudHeightRange);
    cloudHeight = saturate(cloudHeight);
    info.normalizedHeight = cloudHeight;
    float cloudTypeGradient = saturate(remap(cloudHeight, cloudType.x, cloudType.y, 0.0, 1.0))
                            * saturate(remap(cloudHeight, cloudType.z, cloudType.w, 1.0, 0.0));

    info.dimensionalProfile = cloudTypeGradient * areaMap;
    // float cloudShape = saturate(remap(info.dimensionalProfile, 1 - lowFrequencyFbm, 1.0, 0, 1));
    float cloudShape = saturate(lowFrequencyFbm - (1 - info.dimensionalProfile));

    info.density = cloudShape;
}

float3 LightMarch(float3 pos)
{
    float extinction = 0;
    const float fixedStep = perMaterial.lightStep;
    for(int i = 1; i < perMaterial.lightStepCount; i++)
    {
        pos += -fixedStep * perScene.lights[0].position.xyz;
        CloudInfo cloudInfo = SampleDensity(pos);
        extinction += cloudInfo.density * fixedStep;
    }
    extinction *= perMaterial.lightExtinction;

    return extinction * perScene.lights[0].lightColor.xyz * perMaterial.lightScale;
}


float4 RenderCloud(float3 dir, float3 start, float3 end) {
    const float FixedStep = max(perMaterial.fixedStep, 0.01);

    float4 color = 0;

    float backgroundTransmittance = 1;

    float3 ldir = normalize(-perScene.lights[0].position.xyz);
    float3 lightColor = perScene.lights[0].lightColor.xyz;

    float phase = saturate(DualLobHenyeyGreenstein(perMaterial.lightPhase_g, dot(ldir, dir), perMaterial.lightPhase_K));

    float4 lighting = float4(0,0,0,1);
    float step = length(start - end) * FixedStep;
    [loop]
    for (float v = 0; v < 1; v += FixedStep) {
        float3 positionWS = lerp(start, end, v);
        CloudInfo cloudInfo = SampleDensity(positionWS);
        float extinction = max(cloudInfo.density * step * perMaterial.extinction, 0.00001f);
        float transmittance = Transmittance(extinction);

        // https://advances.realtimerendering.com/s2023/Nubis%20Cubed%20(Advances%202023).pdf
        float3 ambient = float3(0,0.3,0) * pow(1 - cloudInfo.dimensionalProfile, 0.5);
        float3 luminance = LightMarch(positionWS);
        float3 integScattering = (luminance - luminance * transmittance) / extinction;
        lighting.xyz += integScattering * lighting.a;
        lighting.a *= transmittance;
    }

    return float4(lighting.xyz, lighting.a);
}

bool RayPlaneIntersection(float3 origin, float3 dir, float4 plane, out float t)
{
    float3 rayDir = dir;
    float3 rayOrigin = origin;
    float3 planeNorm = plane.xyz;
    float3 planePoint = -plane.xyz * plane.w;

    float denom = dot(planeNorm, rayDir);
    if (abs(denom) < 1e-6)
    {
        return false; // no intersection
    }

    // Calculate the perMaterialeter 't' for the ray
    t = dot(planeNorm, planePoint - rayOrigin) / denom;

    // If t is negative, the intersection is behind the ray's origin
    if (t < 0.0)
    {
        return false; // no intersection
    }

    return true; // intersection occurs
}

float RaySphereIntersect(float3 r0, float3 rd, float3 s0, float sr) {
    // - r0: ray origin
    // - rd: normalized ray direction
    // - s0: sphere center
    // - sr: sphere radius
    // - Returns distance from r0 to first intersecion with sphere,
    //   or -1.0 if no intersection.
    float a = dot(rd, rd);
    float3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
    if (b*b - 4.0*a*c < 0.0) {
        return -1.0;
    }
    return (-b - sqrt((b*b) - 4.0*a*c))/(2.0*a);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= perScene.screenSize.xy))
        return;

    float d = perMaterial.depthMap.Load(uint3(id.xy,0)).x;
    if (d != 1.0)
    {
        return;
    }

    float2 uv = (id.xy + 0.5) * perScene.screenSize.zw;
    float2 ndc = uv * 2 - 1;
    float3 screenCoord = float3(ndc, perMaterial.nearPlane);
    float4 rayPoint = mul(perScene.invNDCToWorld, float4(screenCoord, 1.0));
    rayPoint.xyz /= rayPoint.w;
    float3 dir = normalize(rayPoint.xyz - perScene.viewPos.xyz);
    float t = RaySphereIntersect(perScene.viewPos.xyz, dir, 0, perMaterial.radiusTest);
    if (t < 0)
        return;

    float3 start = perScene.viewPos.xyz + t * dir;
    float4 cloud = RenderCloud(dir, start, start + dir * 10);

    float4 mainColor = perMaterial.mainColor.Load(id.xy);
    perMaterial.mainColor[id.xy] = lerp(float4(cloud.xyz, mainColor.a), mainColor, cloud.w);
}

/**
2024-1-29
if you adjust the stratusType so that lower part has a low difference range the light will be dark, but if the lower part has a high difference range, it will be bright. This indicates that light transmittance is falsely calculated

#ifndef VOLUMETRIC_CLOUD_INCLUDED
#define VOLUMETRIC_CLOUD_INCLUDED



#endif
*/

