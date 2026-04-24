import Library.PerScene;
import WeilanEngine;
// https://www.shadertoy.com/view/wslyWs
float3 Sky(float3 ro, float3 rd, float3 lightDir, bool hasSun)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y; 
    float2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    float3 skyCol = float3(0.3, 0.5, 0.85) - rd.y * rd.y * 0.5;
    skyCol = lerp(skyCol, 0.85 * float3(0.7, 0.75, 0.85), pow(1.0 - max(rd.y, 0.0), 4.0));

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    // sun
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * float3(1.0, 0.7, 0.4) * pow(sundot, 5.0);
        sun += 0.25 * float3(1.0, 0.8, 0.6) * pow(sundot, 64.0);
        sun += 0.2 * float3(1.0, 0.8, 0.6) * pow(sundot, 512.0);
        skyCol += sun;
    }

    // horizon
    skyCol = lerp(skyCol, 0.68 * float3(.418, .394, .372), pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol * 0.45;
}

// Convert cubemap face and UV coordinates to 3D direction
float3 CubemapUVToDirection(uint face, float2 uv)
{
    // UV is in [0, 1] range, convert to [-1, 1]
    float2 ndc = uv * 2.0 - 1.0;
    
    float3 dir;
    switch (face)
    {
        case 0: // +X
            dir = float3(1.0, -ndc.y, -ndc.x);
            break;
        case 1: // -X
            dir = float3(-1.0, -ndc.y, ndc.x);
            break;
        case 2: // +Y
            dir = float3(ndc.x, 1.0, ndc.y);
            break;
        case 3: // -Y
            dir = float3(ndc.x, -1.0, -ndc.y);
            break;
        case 4: // +Z
            dir = float3(ndc.x, -ndc.y, 1.0);
            break;
        case 5: // -Z
            dir = float3(-ndc.x, -ndc.y, -1.0);
            break;
        default:
            dir = float3(0.0, 0.0, 1.0);
            break;
    }
    
    return normalize(dir);
}

struct ComputeInput
{
    float4 resolution; // Cubemap face resolution

    RWTexture2DArray<float4> outputCubemap;
}

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<ComputeInput> computeInput;

// Output cubemap

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    uint face = dispatchThreadID.z;
    
    // Check bounds
    if (pixelCoord.x >= computeInput.resolution.x || pixelCoord.y >= computeInput.resolution.y || face >= 6)
        return;
    
    // Calculate UV coordinates [0, 1]
    float2 uv = (float2(pixelCoord) + 0.5) * computeInput.resolution.zw;
    
    // Get direction for this cubemap pixel
    float3 direction = CubemapUVToDirection(face, uv);
    
    // Shade the sky
    bool hasSun = perScene.scene.lightCount > 0;
    float3 lightDir = hasSun ? perScene.scene.lights[0].position.xyz : float3(0.0, 1.0, 0.0);
    float3 skyColor = saturate(Sky(float3(0.0, 0.0, 0.0), direction, lightDir, hasSun));
    
    // Write to output cubemap
    computeInput.outputCubemap[uint3(pixelCoord, face)] = float4(skyColor, 1.0);
}
