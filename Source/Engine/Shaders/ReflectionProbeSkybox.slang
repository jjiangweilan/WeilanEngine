import Library.PerScene;
import WeilanEngine;
import Sky_Common;

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
    
    float3 skyColor = Sky(perScene, float3(0.0), direction);
    
    // Write to output cubemap
    computeInput.outputCubemap[uint3(pixelCoord, face)] = float4(skyColor, 1.0);
}
