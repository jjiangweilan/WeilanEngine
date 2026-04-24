#if CONFIG
name: DepthDownSampler
#endif

import WeilanEngine;

struct Input
{
    float4 dstTexelSize;

    Texture2D src;
    [format("r32f")]
    RWTexture2D<float> dst;
    SamplerState sampler_linear_clamp;
};

[Material]
ParameterBlock<Input> input;

[shader("compute")]
[numthreads(8, 8, 1)]
void DepthDownSampler(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= input.dstTexelSize.z || dispatchThreadID.y >= input.dstTexelSize.w)
        return;

    float2 uv = (dispatchThreadID.xy + 0.5) * input.dstTexelSize.xy;

    float4 depth = input.src.GatherRed(input.sampler_linear_clamp, uv);
    float farest = min(depth.x, min(depth.y, min(depth.z, depth.w)));
    input.dst[dispatchThreadID.xy] = farest;
}
