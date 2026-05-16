#if CONFIG
name: SSILGeometry
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

struct SSILGeometryParameter
{
    float4 rtSize;     // (halfWidth, halfHeight, 1/halfWidth, 1/halfHeight)
    float4 fullResSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)

    Texture2D depthTex;        // full-res depth (mip 0)
    Texture2D<uint> smoothNormalTex; // full-res packed smooth normal

    [format("rg32ui")]
    RWTexture2D<uint2> outGeometryTex;
    [format("r8ui")]
    RWTexture2D<uint> outCoordOffsetTex;

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILGeometryParameter> perMaterial;

[numthreads(8, 8, 1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 halfPixel = dispatchThreadID.xy;
    if (halfPixel.x >= perMaterial.rtSize.x || halfPixel.y >= perMaterial.rtSize.y)
        return;

    float bestDepth = 0.0;
    uint2 bestOffset = uint2(0, 0);
    bool valid = false;

    [unroll]
    for (int dy = 0; dy < 2; ++dy)
    {
        [unroll]
        for (int dx = 0; dx < 2; ++dx)
        {
            uint2 fullResCoord = halfPixel * 2 + uint2(dx, dy);
            if (fullResCoord.x >= perMaterial.fullResSize.x || fullResCoord.y >= perMaterial.fullResSize.y)
                continue;

            float d = perMaterial.depthTex.Load(int3(fullResCoord, 0)).x;
            if (d > 0.0000001)
            {
                if (!valid || d < bestDepth)
                {
                    bestDepth = d;
                    bestOffset = uint2(dx, dy);
                }
                valid = true;
            }
        }
    }

    if (!valid)
    {
        perMaterial.outGeometryTex[halfPixel] = uint2(0, 0);
        perMaterial.outCoordOffsetTex[halfPixel] = 0;
        return;
    }

    uint2 bestFullResCoord = halfPixel * 2 + bestOffset;
    uint packedNormal = perMaterial.smoothNormalTex.Load(int3(bestFullResCoord, 0)).x;
    perMaterial.outGeometryTex[halfPixel] = uint2(asuint(bestDepth), packedNormal);
    perMaterial.outCoordOffsetTex[halfPixel] = bestOffset.x | (bestOffset.y << 1);
}