#if CONFIG
name: Ocean
cull: none
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

// Gerstner Wave function
float3 GerstnerWave(Wave wave, float3 position, float time, float waveCount, inout float3 tangent, inout float3 binormal)
{
    float k = 2.0 * M_PI / wave.wavelength;
    float c = sqrt(9.8 / k);
    float2 d = normalize(wave.direction);
    float f = k * (dot(d, position.xz) - c * time * wave.speed);
    float a = wave.amplitude / k;
    
    // Steepness factor (Q) - controls how peaked the waves are
    float Q = wave.steepness / (wave.amplitude * k * waveCount);
    
    // Displacement
    float3 offset;
    offset.x = Q * a * d.x * cos(f);
    offset.y = a * sin(f);
    offset.z = Q * a * d.y * cos(f);
    
    // Tangent and binormal contributions for normal calculation
    float WA = wave.amplitude * k;
    float S = sin(f);
    float C = cos(f);
    
    tangent += float3(
        -Q * d.x * d.x * WA * S,
        d.x * WA * C,
        -Q * d.x * d.y * WA * S
    );
    
    binormal += float3(
        -Q * d.x * d.y * WA * S,
        d.y * WA * C,
        -Q * d.y * d.y * WA * S
    );
    
    return offset;
}

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;
    
    float3 worldPos = mul(ps.model, float4(input.position, 1.0)).xyz;
    float time = perScene.scene.time;
    
    // Initialize tangent space vectors
    float3 tangent = float3(1, 0, 0);
    float3 binormal = float3(0, 0, 1);
    float3 waveOffset = float3(0, 0, 0);
    
    // Accumulate Gerstner waves
    uint waveCount = uint(mat.waveCount);
    
    for (uint i = 0; i < waveCount; i++)
    {
        Wave wave = mat.waves[i];
        waveOffset += GerstnerWave(wave, worldPos, time, float(waveCount), tangent, binormal);
    }
    
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
        float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float VdotH = max(dot(V, H), 0.0);
        
        // Fresnel (Schlick approximation)
        float3 F0 = lerp(float3(0.02), baseColor, metallic); // Water F0 is about 0.02
        float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
        
        // GGX Distribution
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        float denom = NdotH * NdotH * (alpha2 - 1.0) + 1.0;
        float D = alpha2 / (M_PI * denom * denom);
        
        // Smith Geometry
        float k = alpha / 2.0;
        float G1_V = NdotV / (NdotV * (1.0 - k) + k);
        float G1_L = NdotL / (NdotL * (1.0 - k) + k);
        float G = G1_V * G1_L;
        
        // Cook-Torrance BRDF
        float3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);
        
        // Diffuse with subsurface approximation
        float3 kD = (1.0 - F) * (1.0 - metallic);
        float3 diffuse = kD * baseColor / M_PI;
        
        // Subsurface scattering approximation
        float subsurface = pow(saturate(dot(V, -L)), 2.0) * subsurfaceStrength;
        float3 subsurfaceColor = float3(0.1, 0.4, 0.5) * subsurface;
        
        // Light attenuation for point lights
        float attenuation = 1.0;
        if (lightPos.a != 0.0) // Point light
        {
            float distance = length(lightPos.xyz - input.positionWS);
            attenuation = 1.0 / (1.0 + distance * distance * 0.1);
        }
        
        // Combine lighting
        float3 radiance = light.lightColor.xyz * light.intensity * attenuation;
        finalColor += (diffuse + specular + subsurfaceColor) * radiance * NdotL;
    }
    
    // Ambient lighting with skybox-like fallback
    float3 ambient = float3(0.03, 0.05, 0.1) * baseColor;
    finalColor += ambient;
    
    // Fresnel-based transparency for nice water edge effects
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float alpha = lerp(0.85, 0.95, fresnel);
    
    // Add foam tips on steep waves
    float foamThreshold = 0.7;
    float foam = smoothstep(foamThreshold, 1.0, abs(N.y));
    finalColor = lerp(finalColor, float3(1.0), foam * 0.3);
    
    return float4(finalColor, alpha);
}

