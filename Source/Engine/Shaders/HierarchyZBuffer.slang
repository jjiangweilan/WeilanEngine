#if CONFIG
name: HierarchyZBuffer
#endif

import WeilanEngine; //

struct Input
{
    float4 srcTexelSize; // xy: 1/w, 1/h; zw: w, h
    float4 dstTexelSize; // xy: 1/w, 1/h; zw: w, h

    Texture2D src;
    [format("rg32f")]
    RWTexture2D<float2> dst;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void GenerateMip0(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 dstSize = uint2(input.dstTexelSize.zw);
    if (dispatchThreadID.x >= dstSize.x || dispatchThreadID.y >= dstSize.y)
        return;

    float depth = input.src.Load(int3(dispatchThreadID.xy, 0)).r;
    input.dst[dispatchThreadID.xy] = float2(depth, depth);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void HierarchyZBuffer(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 dstSize = uint2(input.dstTexelSize.zw);
    if (dispatchThreadID.x >= dstSize.x || dispatchThreadID.y >= dstSize.y)
        return;

    uint2 srcSize = uint2(input.srcTexelSize.zw);
    uint2 base = dispatchThreadID.xy * 2;

    // Use 3x3 overlap only when dimensions are odd
    uint xCount = (srcSize.x & 1) ? 3 : 2;
    uint yCount = (srcSize.y & 1) ? 3 : 2;

    float minDepth = 1.0;
    float maxDepth = 0.0;

    for (uint y = 0; y < yCount; ++y)
    {
        for (uint x = 0; x < xCount; ++x)
        {
            uint2 coord = min(base + uint2(x, y), srcSize - 1);
            float d = input.src.Load(int3(coord, 0)).r;
            minDepth = min(minDepth, d);
            maxDepth = max(maxDepth, d);
        }
    }

    input.dst[dispatchThreadID.xy] = float2(minDepth, maxDepth);
}
