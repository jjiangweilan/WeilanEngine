#if CONFIG
name: HierarchyZBuffer_Mip0
#endif
//
import WeilanEngine;

struct Input
{
    float4 srcTexelSize;
    float4 dstTexelSize;

    Texture2D src;
    [format("rg32f")]
    RWTexture2D<float2> dst;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 dstSize = uint2(input.dstTexelSize.zw);
    if (dispatchThreadID.x >= dstSize.x || dispatchThreadID.y >= dstSize.y)
        return;

    float depth = input.src.Load(int3(dispatchThreadID.xy, 0)).r;
    input.dst[dispatchThreadID.xy] = float2(depth, depth);
}
