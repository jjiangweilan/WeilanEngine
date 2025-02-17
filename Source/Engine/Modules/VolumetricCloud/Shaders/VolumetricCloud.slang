import Library.PerScene;

#if CONFIG
name: Game/Cloud
cull: back
blend:
- one srcAlpha
#endif

struct VolumetricCloudParameter {
    float4 stratusType;
    float4 stratocumulusType;
    float4 cumulusType;
    float4 areaMapRemap;
    float4 ambientColor;
    float cubeSize;
    float nearPlane;
    float maxThickness; // max thickness in normalized scale
    float fixedStep;    // fixed step in normalized scale
    float extinction;
    float lightExtinction;
    float lightPhase_g;
    float lightPhase_K;
    float areaMapPixelWorldSize;
    float detailMapScale;
    float highFrequencyMapScale;
    float ambientScale;
    float lightScale;
    float lightStep;
    float lightStepCount;
    float globalScale;

#if GPU_RESOURCE
    [format("rgba8")]
    RWTexture2D<float4> mainColor;
    Texture3D cloudDensity;
    Texture3D highFrequencyCloudDensity;
    Texture2D depthMap;
    Texture2D interleavedGradientNoise;
    SamplerState sampler_Linear_Repeat;
    SamplerState sampler_Linear_Clamp;
#endif
};

float3 GetCloudMin()
{
    return float3(-perMaterial.cubeSize, -10, -perMaterial.cubeSize);
}

float3 GetCloudMax()
{
    return float3(perMaterial.cubeSize, 10, perMaterial.cubeSize);
}

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
float PowderEffect(float d, float costh) { return 1 - exp(-d*2) ; }

float HenyeyGreenstein(float g, float costh) {
    const float pi = 3.14159265358979323846;
    return (1.0 - g * g) /
        (4.0 * pi * pow(1.0 + g * g - 2.0 * g * costh, 3.0 / 2.0));
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

bool RayIntersectsBox(float3 rayOrigin, float3 rayDirection, float3 boxMin, float3 boxMax, out float tMin, out float tMax) {
    float t1 = (boxMin.x - rayOrigin.x) / rayDirection.x;
    float t2 = (boxMax.x - rayOrigin.x) / rayDirection.x;
    tMin = min(t1, t2);
    tMax = max(t1, t2);
    
    t1 = (boxMin.y - rayOrigin.y) / rayDirection.y;
    t2 = (boxMax.y - rayOrigin.y) / rayDirection.y;
    tMin = max(tMin, min(t1, t2));
    tMax = min(tMax, max(t1, t2));
    
    t1 = (boxMin.z - rayOrigin.z) / rayDirection.z;
    t2 = (boxMax.z - rayOrigin.z) / rayDirection.z;
    tMin = max(tMin, min(t1, t2));
    tMax = min(tMax, max(t1, t2));
    
    return tMax >= max(tMin, 0.0);
}

bool IsPosInsideCloud(float3 pos)
{
    return pos.x >= GetCloudMin().x && pos.x <= GetCloudMax().x &&
           pos.y >= GetCloudMin().y && pos.y <= GetCloudMax().y &&
           pos.z >= GetCloudMin().z && pos.z <= GetCloudMax().z;
}

CloudInfo SampleDensity(float3 worldPos)
{
    CloudInfo info = CloudInfo(0,0,0);

    if (IsPosInsideCloud(worldPos))
    {
        float3 samplePos = (worldPos) / perMaterial.detailMapScale;
        float4 density = perMaterial.cloudDensity.SampleLevel(perMaterial.sampler_Linear_Repeat, samplePos.xyz, 0);
        // info.dimensionalProfile *= density.x;

        float areaMap = saturate(remap(density.r, perMaterial.areaMapRemap.x, perMaterial.areaMapRemap.y, 0, 1));
        const float4 cloudType = perMaterial.stratusType;
        float cloudHeight = saturate((worldPos.y + 10) / 20);
        float cloudTypeGradient = saturate(remap(cloudHeight, cloudType.x, cloudType.y, 0.0, 1.0))
            * saturate(remap(cloudHeight, cloudType.z, cloudType.w, 1.0, 0.0));

        info.normalizedHeight = cloudHeight;
        areaMap *= cloudTypeGradient;

        float lowFrequencyFbm = 0.625 * density.y + 0.25 * density.z + 0.125 * density.w;
        info.dimensionalProfile = saturate(remap(areaMap, 1 - lowFrequencyFbm, 1.0, 0, 1)) ;


        float3 highFrequencySamplePos = (worldPos) / perMaterial.highFrequencyMapScale;
        float3 highFrequencyNoises = perMaterial.highFrequencyCloudDensity.SampleLevel(perMaterial.sampler_Linear_Repeat, highFrequencySamplePos.xyz, 0).yzw;
        float highFrequencyFbm = 0.625 * highFrequencyNoises.x;// + 0.25 * highFrequencyNoises.y + 0.125 * highFrequencyNoises.z;
        float highFrequencyNoiseModifier = lerp(highFrequencyFbm, 1 - highFrequencyFbm, saturate(cloudTypeGradient * 10));

        info.density = saturate(remap(info.dimensionalProfile, 1 - highFrequencyNoiseModifier, 1, 0, 1));
        // info.density = saturate(lowFrequencyFbm - (1 - info.dimensionalProfile));

        // info.density = info.dimensionalProfile;
    }


    return info;
}

float3 LightMarch(float3 pos, float mu)
{
    float extinction = 0;
    const float fixedStep = perMaterial.lightStep;
    for(int i = 1; i < perMaterial.lightStepCount; i++)
    {
        CloudInfo cloudInfo = SampleDensity(pos);
        extinction += cloudInfo.density * fixedStep;
        pos += fixedStep * perScene.lights[0].position.xyz;
    }
    extinction *= perMaterial.lightExtinction;

    float powderEffect = 1.0f;//PowderEffect(extinction, mu);
    return Transmittance(extinction) * powderEffect * perScene.lights[0].lightColor.xyz * perMaterial.lightScale;
}


float4 RenderCloud(float3 dir, float3 start, float3 end) {
    const float FixedStep = max(perMaterial.fixedStep, 0.001);

    float4 lighting = float4(0,0,0,1);
    float step = length(start - end) * perMaterial.fixedStep;
    [loop]
    for (float v = 0.01f; v < 1; v += FixedStep) {
        float3 positionWS = lerp(start, end, v);
        CloudInfo cloudInfo = SampleDensity(positionWS);
        if (cloudInfo.density > 0)
        {
            float extinction = max(cloudInfo.density * perMaterial.extinction, 0.0000001f);
            float transmittance = Transmittance(extinction * step);

            float3 ambient =  perMaterial.ambientScale * perMaterial.ambientColor.xyz * pow(1.0f -  cloudInfo.dimensionalProfile, 0.5) * lerp(0.3, 0.5, cloudInfo.normalizedHeight);
            float3 ldir = normalize(perScene.lights[0].position.xyz * 1000000 - positionWS);
            float mu = dot(ldir, dir);
            float phase = saturate(DualLobHenyeyGreenstein(perMaterial.lightPhase_g, mu, perMaterial.lightPhase_K));
            float3 luminance = phase * LightMarch(positionWS, mu) + ambient;

            // https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
            float3 integScattering = (luminance - luminance * transmittance) / extinction;
            lighting.xyz += integScattering * lighting.a;
            lighting.a *= transmittance;
        }
    }

    return float4(lighting.xyz * perMaterial.globalScale, lighting.a);
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

    float noiseOffset = (perMaterial.interleavedGradientNoise.Load(int3(id.xy % 32, 0)).x) ;
    float tmin, tmax;
    bool intersected = RayIntersectsBox(perScene.viewPos.xyz, dir, GetCloudMin(), GetCloudMax(), tmin, tmax);
    tmin = max(tmin, 0.001);
    // float t = RaySphereIntersect(perScene.viewPos.xyz, dir, 0, perMaterial.radiusTest) + noiseOffset;
    if (!intersected)
        return;

    tmin += noiseOffset;
    float3 start = perScene.viewPos.xyz + tmin * dir;
    float3 end = perScene.viewPos.xyz + tmax * dir;
    float4 cloud = RenderCloud(dir, start, end);

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

