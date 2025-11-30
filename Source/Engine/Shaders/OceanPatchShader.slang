import WeilanEngine;
import Library.PerScene;

#include "GerstnerWave.hlsl"

// should match the declaration in OceanRenderer
struct GPUNodeInstanceData
{
    float2 position;
    float2 scale;
};

struct InputParams
{
    StructuredBuffer<GPUNodeInstanceData> instanceData;
};

struct Attributes
{
    float3 position : POSITION;
};

struct Varyings
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 waveOffset;
};

struct DrawParams
{
    int lod;
};

struct MaterialInput
{
    float4 waveColorBlend;
    [[Color]] float4 waveTopColor;
    [[Color]] float4 waveBottomColor;
    float areaScale;
    float yOffset;
    int waveCount;
    StructuredBuffer<Wave> waves;
}

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<MaterialInput> mat;

[Object]
ParameterBlock<InputParams> params;

[vk_push_constant]
DrawParams drawParams;

[shader("vertex")]
Varyings Vertex(Attributes input, int instanceID :  SV_InstanceID, int baseInstance : SV_StartInstanceLocation)
{
    var instanceData = params.instanceData[baseInstance + instanceID];

    float3 positionWS = input.position;
    positionWS.y = mat.yOffset;
    positionWS.xz *= instanceData.scale;
    positionWS.xz += instanceData.position;

    float time = perScene.scene.time;
    float3 normalWS = float3(0,0,0);
    float3 waveOffset = float3(0,0,0);
    positionWS = GerstenerWave(positionWS, mat.waves, mat.waveCount, mat.areaScale, time, normalWS, waveOffset);

    float4 positionCS = perScene.WorldToClipSpace(positionWS);

    Varyings output;
    output.position = positionCS;
    output.positionWS = positionWS;
    output.normalWS = normalWS;
    output.waveOffset = waveOffset;
    return output;
}

[shader("fragment")]
float4 Fragment(Varyings input) : SV_Target
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
