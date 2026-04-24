import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;

struct S2HDebugParameter
{
    Texture2D<float4> debugTex;
    Texture2D<float> depthTex;
    RWTexture2D<float4> outColorTex;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<S2HDebugParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 texCoord = dispatchThreadID.xy;
    
    // Load debug value
    float4 debugVal = perMaterial.debugTex.Load(int3(texCoord, 0));
    
    // Check if anything was drawn (alpha > 0 or whatever depth condition)
    if (debugVal.w > 0.0)
    {
        float sceneDepth = perMaterial.depthTex.Load(int3(texCoord, 0));
        
        // Depth test (Reversed-Z: debug depth must be >= scene depth to be visible)
        if (debugVal.w >= sceneDepth)
        {
            perMaterial.outColorTex[texCoord] = float4(debugVal.xyz, 1.0);
        }
    }
}