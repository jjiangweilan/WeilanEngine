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

#if GPU_RESOURCE
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

float Extinction(float d) { return exp(-1 * d) ; }

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

void SampleDensity(float3 worldPos, out CloudInfo info)
{
    float2 areaMapUV =( worldPos.xz) * perMaterial.cloudAreaMap_TexelSize.xy / perMaterial.areaMapPixelWorldSize + 0.5;
    float samplePointDistance = length(worldPos - perScene.viewPos.xyz);
    float areaMapLod = samplePointDistance / 30000;
    float4 areaMapTex = perMaterial.cloudAreaMap.SampleLevel(perMaterial.sampler_Linear_Repeat, areaMapUV, 0);
    float areaMap = saturate(remap(areaMapTex.x,
        perMaterial.areaMapRemap.x, perMaterial.areaMapRemap.y, perMaterial.areaMapRemap.z, perMaterial.areaMapRemap.w));

    float3 detailSamplePos = (worldPos) / perMaterial.detailMapScale;
    float4 density = perMaterial.cloudDensity.SampleLevel(perMaterial.sampler_Linear_Repeat, detailSamplePos.xzy, 0);
    if (areaMapLod > 8)
    {
        density = max(areaMap, 0.3);
    }
    float lowFrequencyFbm = 0.625 * density.y + 0.25 * density.z + 0.125 * density.w;

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


float4 RenderCloud(float3 dir, float3 start, float3 end) {
    const float FixedStep = max(perMaterial.fixedStep, 0.01);

    float4 color = 0;

    float backgroundTransmittance = 1;

    float3 ldir = normalize(perScene.lights[0].position.xyz);
    float3 lightColor = perScene.lights[0].lightColor.xyz;

    float phase = saturate(DualLobHenyeyGreenstein(perMaterial.lightPhase_g, dot(ldir, dir), perMaterial.lightPhase_K));
    float totalTransmittance = 1.0f;
    float3 lighting = 0;
    CloudInfo cloudInfo;
    [loop]
    for (float v = 0; v < 1; v += FixedStep) {
        float3 positionWS = lerp(start, end, v);
        float dimensionalProfile;
        SampleDensity(positionWS, cloudInfo);
        if (cloudInfo.density > 0)
        {
            float d = cloudInfo.density * FixedStep * perMaterial.extinction;
            float transmittance = Extinction(d);
            totalTransmittance *= transmittance;

            if (totalTransmittance < 0.005)
                break;

            // marching towards light
            const float lightSampleStep = 0.05;
            const float maxLightSampleStepCount = 16;
            float3 lightSampleWS = positionWS + ldir * lightSampleStep;
            float lightTotalDensity = 0;
            for(float lv = 0; lv < maxLightSampleStepCount; lv++)
            {
                CloudInfo cloudInfo2;
                SampleDensity(lightSampleWS, cloudInfo2);
                float lightSampleDensity = cloudInfo2.density;

                if (lightSampleDensity > 0)
                {
                    float dd = lightSampleDensity * lightSampleStep;
                    lightTotalDensity += dd;
                }
            }
            float light_d = lightTotalDensity * perMaterial.lightExtinction;
            float lightTransmittance = saturate(Extinction(light_d));

            float3 ambient = lightColor * lerp(perMaterial.ambientHeightScale.x, perMaterial.ambientHeightScale.y, cloudInfo.normalizedHeight);
            float3 luminance = ambient + lightColor * phase * lightTransmittance;

            // energy conservation https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
            lighting +=
                totalTransmittance * (luminance - luminance * transmittance) / perMaterial.lightExtinction;
        }
    }

    totalTransmittance = max(0, totalTransmittance);

    return float4(lighting, 1 - totalTransmittance);
    // float fog = saturate(Extinction(length(start - _WorldSpaceCameraPos.xyz) / 50000));
    // return float4(lerp(float3(0.8,0.8,0.8), lighting, fog), 1 - totalTransmittance);
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

float RayVsSphere(float3 ro, float3 rd, float3 sph, float rad) {
 	float3 oc = ro - sph.xyz;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - rad*rad;
    float t = b*b - c;
    if( t > 0.0)
        t = -b - sqrt(t);
    return t;

}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= perScene.screenSize.xy))
        return;

    float d = perMaterial.depthMap.Load(uint3(id.xy,0)).x;
    if (d != 0.0)
    {
        perMaterial.mainColor[id.xy] = perMaterial.mainColor.Load(id.xy);
        return;
    }

    float2 uv = (id.xy + 0.5) * perScene.screenSize.zw;
    float2 ndc = uv * 2 - 1;
    ndc.y = - ndc.y;
    float3 screenCoord = float3(ndc, perMaterial.nearPlane);
    float4 worldPos = mul(perScene.invNDCToWorld, float4(screenCoord, 1.0));
    worldPos.xyz /= worldPos.w;

    float3 start = worldPos.xyz;
    float3 dir = normalize(worldPos.xyz - perScene.viewPos.xyz);

    float4 cloud = 0;

    float3 vecDir = dir / max(dir.y, 0.00001f);
    start = float3(start.x, 0, start.z) + perMaterial.cloudPlane.w * vecDir;
    float3 end = start + perMaterial.maxThickness * vecDir;

    cloud = RenderCloud(dir, start, end);

    float4 mainColor = perMaterial.mainColor.Load(id.xy);
    perMaterial.mainColor[id.xy] = lerp(mainColor, cloud, cloud.w);
}

/**
2024-1-29
if you adjust the stratusType so that lower part has a low difference range the light will be dark, but if the lower part has a high difference range, it will be bright. This indicates that light transmittance is falsely calculated

#ifndef VOLUMETRIC_CLOUD_INCLUDED
#define VOLUMETRIC_CLOUD_INCLUDED



#endif
*/

