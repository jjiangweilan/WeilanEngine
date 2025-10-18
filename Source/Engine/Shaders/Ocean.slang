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
    float3 tangentWS;
    float3 bitangentWS;
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
};

[vk_push_constant]
PushConstant ps;

struct MaterialInput
{
    int waveCount;
    StructuredBuffer<Wave> waves;
}

[[Material]]
ParameterBlock<MaterialInput> mat;

float3 GerstnerWave(Wave wave, float3 position, float time, inout float3 tangent, inout float3 binormal)
{
    float2 d = wave.direction;
    float Q = wave.steepness;
    float A = wave.amplitude;
    float W = 1.0 / wave.wavelength;
    float speed = wave.speed;

    float phase = 2 * M_PI * speed * W;
    float f = (W * dot(d, position.xz) + phase * time);
    float QA = Q * A;
    
    // Calculate the wave displacement
    float3 result = float3(0.0, 0.0, 0.0);
    result.x = QA * d.x * cos(f);
    result.y = A * sin(f);
    result.z = QA * d.y * cos(f);

    tangent = float3(1,0,0);
    binormal = float3(0,1,0);

    float s0 = 1;
    float c0 = 1;
    float WA = 1;

    // Calculate binormal vector (derivative with respect to x)
    binormal.x = Q * d.x * d.x * WA * s0;// -d.x * d.y * steepness * sin(f);
    binormal.y = d.x * WA * c0;
    binormal.z = Q * d.x * d.y * WA * s0;
    binormal = normalize(binormal); // ??? normalize here
    
    // Calculate tangent vector (derivative with respect to z)
    tangent.x = Q * d.x * d.y * WA * s0;
    tangent.y = d.y * WA * c0;
    tangent.z = Q * d.y * d.y * WA * s0;
    tangent = normalize(tangent);
    

    return result;
}

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    
    float3 worldPos = mul(ps.model, float4(input.position, 1.0)).xyz;
    float time = perScene.scene.time;
    
    // Initialize tangent space vectors
    float3 tangent = float3(0, 0, 0);
    float3 binormal = float3(0, 0, 0);
    float3 waveOffset = float3(0, 0, 0);
    
    // Accumulate Gerstner waves
    uint waveCount = uint(mat.waveCount);
    
    for (uint i = 0; i < waveCount; i++)
    {
        Wave wave = mat.waves[i];
        float3 outTanget;
        float3 outBinormal;
        waveOffset += GerstnerWave(wave, worldPos, time, outTanget, outBinormal);
        tangent += outTanget;
        binormal += outBinormal;
    }

    binormal = normalize(float3(1 - binormal.x, binormal.y, -binormal.z));
    tangent = normalize(float3(-tangent.x, tangent.y, 1 - tangent.z));
    
    // Apply wave displacement
    worldPos += waveOffset;
    output.positionWS = worldPos;
    
    // Calculate normal from tangent and binormal
    float3 normal = normalize(cross(binormal, tangent));
    output.normalWS = normal;
    output.tangentWS = normalize(tangent);
    output.bitangentWS = normalize(binormal);
    
    output.uv = input.uv;
    output.position = mul(perScene.camera.viewProjection, float4(worldPos, 1.0));
    
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
    
    // Ambient lighting with skybox-like fallback
    float3 ambient = float3(0.03, 0.05, 0.1) * baseColor;
    finalColor += ambient;
    
    // Fresnel-based transparency for nice water edge effects
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float alpha = lerp(0.85, 0.95, fresnel);
    
    return float4(N, alpha);
}

