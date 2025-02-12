import Library.PerScene;

#if CONFIG
name: Game/Cloud
cull: back
blend:
- one srcAlpha
#endif


/*
      y---z
     /     \
   /        \
  x          w
*/
typealias CloudType = float4;

struct Cloud
{
    float4 cloudPlane;
    float4 stratusType;
    float4 stratocumulusType;
    float4 cumulusType;
    float4 areaMapRemap;
    float4 planet;
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

#if GPU_RESOURCE
    Texture3D cloudDensity;
    Texture2D cloudSettings;
    SamplerState sampler_Linear_ClampToBorder;

#endif
};

#include "Utils/Util.hlsl"

#define M_PI 3.1415926

ParameterBlock<PerScene> perScene;

struct VertexOutput {
    float4 position : SV_Position;
    float3 positionWS;
};

struct Transform
{
    float4x4 model;
};
[vk::push_constant]
Transform transform;

struct VertexInput
{
    float3 position;
};

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;

    float3 positionWS = mul(transform.model, float4(input.position, 1)).xyz;
    output.positionWS = positionWS.xyz;
    output.position = mul(perScene.viewProjection, float4(positionWS, 1));

    return output;
}

ParameterBlock<Cloud> perMaterial;

float3 WSNormalization(float3 pos)
{
    return (pos - perMaterial.cubePos.xyz) / perMaterial.cubeExtent.xyz;
}


float Transmittance(float d)
{
    return exp(-1 * d);
}

float PowderSugar(float d)
{
    return 1.0 - exp(-2*d);
}

float SampleDensity(float3 normalizedPos)
{
    float3 samplePos = normalizedPos * perMaterial.sampleScale.xyz;

    float4 density = perMaterial.cloudDensity.Sample(perMaterial.sampler_Linear_ClampToBorder, samplePos.xzy);
    float lowFrequencyFbm = 0.425 * density.y + 0.35 * density.z + 0.225 * density.w;
    float cloudHeight = normalizedPos.y;
    const float4 cloudType = perMaterial.stratusType;
    float cloudTypeGradient = saturate(remap(cloudHeight, cloudType.x, cloudType.y, 0.0, 1.0))
                            * saturate(remap(cloudHeight, cloudType.z, cloudType.w, 1.0, 0.0));

    float baseCloudShape = density.x * cloudTypeGradient;
    float cloudShape = saturate(remap(baseCloudShape, lowFrequencyFbm, 1.0, 0, 1));

    return cloudShape;
}


// -1 < g < 1, 0.8 for clouds
float HenyeyGreenstein(float g, float costh)
{
    return (1.0 - g * g) / (4.0 * M_PI * pow(1.0 + g * g - 2.0 * g * costh, 1.5f));
}

float DualLobHenyeyGreenstein(float g, float costh, float K)
{
    return lerp(HenyeyGreenstein(-g, costh), HenyeyGreenstein(g, costh), K);
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    const float maxThickness = max(perMaterial.maxThickness, 0.01);
    const float fixedStep = max(perMaterial.fixedStep, 0.01);

    float3 positionWS = (input.positionWS.xyz);
    float3 viewPos = (perScene.viewPos.xyz);

    float3 dir = normalize(positionWS - viewPos);
    float3 start = positionWS;
    float3 end = start + dir * maxThickness;

    float4 color = 0;

    float3 ldir = perScene.lights[0].position.xyz;
    float phase = saturate(DualLobHenyeyGreenstein(perMaterial.lightPhase_g, dot(ldir, dir), perMaterial.lightPhase_K));

    float totalTransmittance = 1;
    float3 lighting = 0;
    for (float v = 0; v < maxThickness; v += fixedStep)
    {
        float3 postionWS = lerp(start, end, v / maxThickness);
        float3 normalizedPos = WSNormalization(postionWS);
        float density = SampleDensity(normalizedPos);

        if (density > 0)
        {
            float d = density * fixedStep * perMaterial.extinction;
            float transmittance = Transmittance(d);
            totalTransmittance *= transmittance;

            if (totalTransmittance < 0.005)
                break;

            // marching towards light
            const float lightSampleStep = 0.05;
            const float maxLightSampleStepCount = 16;
            float3 lightSampleWS = postionWS + ldir * lightSampleStep;
            float lightTotalDensity = 0;
            for(float lv = 0; lv < maxLightSampleStepCount; lv++)
            {
                float3 lightNormalizedPos = WSNormalization(lightSampleWS);
                float lightSampleDensity = SampleDensity(lightNormalizedPos);

                if (lightSampleDensity > 0)
                {
                    float dd = lightSampleDensity * lightSampleStep;
                    lightTotalDensity += dd;
                }
            }
            float light_d = lightTotalDensity * perMaterial.lightExtinction;
            float lightTransmittance = saturate(Transmittance(light_d));

            float3 ambient = perScene.lights[0].lightColor.xyz * lerp(0.3, 0.7, normalizedPos.y);
            float3 luminance = ambient + perScene.lights[0].lightColor.xyz * phase * lightTransmittance;

            // energy conservation https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
            lighting += 
                totalTransmittance * (luminance - luminance * transmittance) / perMaterial.lightExtinction; 

        }
    }

    totalTransmittance = max(0, totalTransmittance);

    return float4(lighting, totalTransmittance);
}

/**
2024-1-29
if you adjust the stratusType so that lower part has a low difference range the light will be dark, but if the lower part has a high difference range, it will be bright. This indicates that light transmittance is falsely calculated

#ifndef VOLUMETRIC_CLOUD_INCLUDED
#define VOLUMETRIC_CLOUD_INCLUDED

#define CloudType float4

struct VolumetricCloudParameter {
    float4 cloudPlane;
    float4 stratusType;
    float4 stratocumulusType;
    float4 cumulusType;
    float4 areaMapRemap;
    float4 planet;
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
};


Texture3D _CloudDensity;
Texture2D _CloudAreaMap;
Texture2D _DepthMap;
SamplerState _Sampler_Linear_Repeat;
SamplerState _Sampler_Linear_Clamp;
float4 _WorldSpaceLightPos0;
float4 _LightColor0;
float4 _CloudAreaMap_TexelSize;
float4x4 _ScreenToWorldMatrix;

StructuredBuffer<VolumetricCloudParameter> _Param;
#define _Param _Param[0]
RWTexture2D<float4> _Output;
Texture2D _MainColor;

float _NearPlane;

float remap(float value, float originMin, float originMax, float newMin,
        float newMax) {
    return newMin +
        (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

float CloudType_Get(float y, CloudType type) {
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
    float2 areaMapUV =( worldPos.xz) * _CloudAreaMap_TexelSize.xy / _Param.areaMapPixelWorldSize + 0.5;
    float samplePointDistance = length(worldPos - _WorldSpaceCameraPos.xyz);
    float2 areaMapTimeOffset = 0 * float2(_Time.xx) * 0.02;
    float3 detailMapTimeOffset = 0 * float3(_Time.x, 0, _Time.x) * 0.05;
    float areaMapLod = samplePointDistance / 30000;
    float4 areaMapTex = _CloudAreaMap.SampleLevel(_Sampler_Linear_Repeat, (areaMapUV + areaMapTimeOffset), 0);
    float areaMap = saturate(remap(areaMapTex.x,
        _Param.areaMapRemap.x, _Param.areaMapRemap.y, _Param.areaMapRemap.z, _Param.areaMapRemap.w));

    float3 detailSamplePos = (worldPos) / _Param.detailMapScale;
    float4 density = _CloudDensity.SampleLevel(_Sampler_Linear_Repeat, detailSamplePos.xzy + detailMapTimeOffset, 0);
    if (areaMapLod > 8)
    {
        density = max(areaMap, 0.3);
    }
    float lowFrequencyFbm = 0.625 * density.y + 0.25 * density.z + 0.125 * density.w;

    const float4 cloudType = _Param.stratusType;
    float cameraDistance = length(worldPos - _WorldSpaceCameraPos.xyz);
    float cloudHeight =  max(0, worldPos.y - _Param.cloudPlane.w) / _Param.cloudHeightRange;
    cloudHeight += cameraDistance / (_Param.cloudSphereBend * _Param.cloudHeightRange);
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
    const float FixedStep = max(_Param.fixedStep, 0.01);

    float4 color = 0;

    float backgroundTransmittance = 1;

    float3 ldir = normalize(_WorldSpaceLightPos0.xyz);
    float3 lightColor = _LightColor0.xyz;

    float phase = saturate(DualLobHenyeyGreenstein(_Param.lightPhase_g, dot(ldir, dir), _Param.lightPhase_K));
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
            float d = cloudInfo.density * FixedStep * _Param.extinction;
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
            float light_d = lightTotalDensity * _Param.lightExtinction;
            float lightTransmittance = saturate(Extinction(light_d));

            float3 ambient = lightColor * lerp(_Param.ambientHeightScale.x, _Param.ambientHeightScale.y, cloudInfo.normalizedHeight);
            float3 luminance = ambient + lightColor * phase * lightTransmittance;

            // energy conservation https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf
            lighting +=
                totalTransmittance * (luminance - luminance * transmittance) / _Param.lightExtinction;
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

    // Calculate the parameter 't' for the ray
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

[numthreads(8, 8, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= _ScreenSize.xy))
        return;

    float d = _DepthMap.Load(uint3(id.xy,0)).x;
    if (d != 0.0)
    {
        _Output[id.xy] = _MainColor.Load(uint3(id.xy, 0));
        return;
    }

    float2 uv = (id + 0.5) * _ScreenSize.zw;
    float2 ndc = uv * 2 - 1;
    ndc.y = - ndc.y;
    float3 screenCoord = float3(ndc, _NearPlane);
    float4 worldPos = mul(_ScreenToWorldMatrix, float4(screenCoord, 1.0));
    worldPos.xyz /= worldPos.w;

    float3 start = worldPos.xyz;
    float3 dir = normalize(worldPos.xyz - _WorldSpaceCameraPos.xyz);

    float4 cloud = 0;
    {
        float3 vecDir = dir / max(dir.y, 0.00001f);
        start = float3(start.x, 0, start.z) + _Param.cloudPlane.w * vecDir;
        float3 end = start + _Param.maxThickness * vecDir;

        cloud = RenderCloud(dir, start, end);
    }

    float4 mainColor = _MainColor.SampleLevel(_Sampler_Linear_Clamp, uv, 0);
    _Output[id.xy] = lerp(mainColor, cloud, cloud.w);
}


#endif
*/

