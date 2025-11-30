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
    float areaScale;
    float4 waveColorBlend;
    [[Color]] float4 waveTopColor;
    [[Color]] float4 waveBottomColor;
    StructuredBuffer<Wave> waves;
}

[[Material]]
ParameterBlock<MaterialInput> mat;

#include "./GerstnerWave.hlsl"

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    VertexOutput output;

    float3 worldPos = mul(ps.model, float4(input.position.xyz, 1.0)).xyz;
    float time = perScene.scene.time;

    float3 normal = float3(0,0,0);
    float3 waveOffset = float3(0,0,0);
    output.positionWS = GerstnerWave(worldPos, mat.waves, mat.waveCount, mat.areaScale, time, normal, waveOffset);

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

    return float4(color, 1);
}

