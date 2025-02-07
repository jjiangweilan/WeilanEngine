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
    float4 cubePos; // left bottom position
    float4 cubeExtent; // cube extent
    float4 sampleScale;
    CloudType stratusType;
    CloudType stratocumulusType;
    CloudType cumulusType;
    float4 baseClouseShapeRemap;
    float maxThickness; // max thickness in normalized scale
    float fixedStep; // fixed step in normalized scale
    float extinction;
    float lightExtinction;
    float lightPhase_K;
    float lightPhase_g;

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
*/
