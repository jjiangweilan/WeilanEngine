#if CONFIG
name: Ocean
cull: none
blend:
    - srcAlpha oneMinusSrcAlpha
#endif

import WeilanEngine;
import Library.PerScene;
import Lighting;

#include "OceanInput.hlsl"

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 waveOffset;
    float2 uv;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

[[Global]]
ParameterBlock<PerScene> perScene;

struct PushConstant
{
    float4x4 model;
    float4x4 invModel;
};

[vk_push_constant]
PushConstant ps;

struct MaterialInput
{
    int waveCount;
    float4 waveColorBlend;
    [[Color]]
    float4 waveTopColor;
    [[Color]]
    float4 waveBottomColor;
    StructuredBuffer<Wave> waves;
}

[[Material]]
ParameterBlock<MaterialInput> mat;

// Tanget and Binornal with cross for Normal later
// after accumulation, tanget and binormal can be calculated as follow
//binormal = normalize(float3(1 - binormal.x, binormal.y, -binormal.z));
//tangent = normalize(float3(-tangent.x, tangent.y, 1 - tangent.z));
float3 GerstnerWave(Wave wave, float3 position, float time, float waveNumber, inout float3 tangent, inout float3 binormal)
{
    float2 d = wave.direction;
    float A = wave.amplitude;
    float W = 2 * M_PI / wave.wavelength;
    float WA = W * A;
    float Q = wave.steepness / (WA * waveNumber);
    float speed = wave.speed * sqrt(9.8 / W); // use deep water theory to calculate speed

    float phase = speed * W;
    float f = (W * dot(d, position.xz) + phase * time);
    float QA = Q * A;
    
    // Calculate the wave displacement
    float3 result = float3(0.0, 0.0, 0.0);
    result.x = QA * d.x * cos(f);
    result.z = QA * d.y * cos(f);
    result.y = A * sin(f);

    tangent = float3(1,0,0);
    binormal = float3(0,1,0);

    float s0 = sin(f);
    float c0 = cos(f);

    // Calculate binormal vector (derivative with respect to x)
    binormal.x = Q * d.x * d.x * WA * s0;// -d.x * d.y * steepness * sin(f);
    binormal.z = Q * d.x * d.y * WA * s0;
    binormal.y = d.x * WA * c0;
    
    // Calculate tangent vector (derivative with respect to z)
    tangent.x = Q * d.x * d.y * WA * s0;
    tangent.z = Q * d.y * d.y * WA * s0;
    tangent.y = d.y * WA * c0;

    return result;
}

float3 GerstnerWave(Wave wave, float3 position, float time, float waveNumber, inout float3 normal)
{
    float2 d = wave.direction;
    float A = wave.amplitude;
    float W = 2 * M_PI / wave.wavelength;
    float WA = W * A;
    float Q = wave.steepness / (WA * waveNumber);
    float speed = wave.speed * sqrt(9.8 / W); // use deep water theory to calculate speed

    float phase = speed * W;
    float f = (W * dot(d, position.xz) + phase * time);
    float QA = Q * A;
    
    // Calculate the wave displacement
    float3 result = float3(0.0, 0.0, 0.0);
    result.x = QA * d.x * cos(f);
    result.z = QA * d.y * cos(f);
    result.y = A * sin(f);

    float s0 = sin(f);
    float c0 = cos(f);

    normal.x = d.x * WA * c0;
    normal.z = d.y * WA * c0;
    normal.y = Q * WA * s0;

    return result;
}

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    
    float3 objectPos = input.position;
    float time = perScene.scene.time;
    
    // Initialize tangent space vectors
    float3 normal = float3(0,0,0);
    float3 waveOffset = float3(0, 0, 0);
    
    // Accumulate Gerstner waves
    uint waveCount = uint(mat.waveCount);
    
    for (uint i = 0; i < waveCount; i++)
    {
        Wave wave = mat.waves[i];
        float3 outNormal;
        waveOffset += GerstnerWave(wave, objectPos, time, waveCount, outNormal);
        normal += outNormal;
    }

    normal = normalize(float3(-normal.x, 1 - normal.y, -normal.z));
    
    // Apply wave displacement
    objectPos += waveOffset;
    output.positionWS = mul(ps.model, float4(objectPos + waveOffset, 1.0)).xyz;
    
    // Calculate normal from tangent and binormal
    // float3 normal = normalize(cross(tangent, binormal));
    output.normalWS = mul(transpose((float3x3)ps.invModel), normal);

    output.uv = input.uv;
    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1.0));
    output.waveOffset = waveOffset;
    
    return output;
}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    // Ocean material properties
    float3 baseColor = float3(0.02, 0.15, 0.3); // Deep ocean blue
    float metallic = 0.0; // Water is non-metallic
    float roughness = 0.05; // Very smooth surface
    float subsurfaceStrength = 0.3;
    float3 waveOffset = input.waveOffset;
    
    // Normalize interpolated vectors
    float3 N = normalize(input.normalWS);
    float3 V = normalize(perScene.camera.position.xyz - input.positionWS);
    
    // Lighting accumulation
    float3 finalColor = float3(0, 0, 0);
    
    // Process each light
    for (int i = 0; i < perScene.scene.lightCount; i++)
    {
        Light light = perScene.scene.lights[i];
        float4 lightPos = light.position;
        
        // Calculate light direction
        float3 L = lightPos.a == 0 ? lightPos.xyz : normalize(lightPos.xyz - input.positionWS);
        float3 H = normalize(L + V);
        
        float NdotL = max(dot(N, L), 0.0);
        finalColor = NdotL * light.lightColor.xyz;
    }

    
    float colorBlend = saturate(waveOffset.y / (mat.waveColorBlend.y - mat.waveColorBlend.x));
    float3 color = lerp(mat.waveBottomColor.xyz, mat.waveTopColor.xyz, colorBlend);

    return float4(finalColor, 1);
}

