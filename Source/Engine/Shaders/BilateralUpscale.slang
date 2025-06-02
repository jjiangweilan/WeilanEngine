#if CONFIG
name: BilateralUpscale
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

struct Input
{
    float4 lowTexelSize; // the target size
    float4 highTexelSize; // the target size
    float kernelScale = 1.0;
    float sigma = 1.0f;

    Texture2D lowColor;
    Texture2D lowdepth;
    Texture2D highDepth;

    RWTexture2D<float> dst;
    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<Input> input;

#define KernelSize 3

[numthreads(8,8,1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= input.highTexelSize.z || pixel.y >= input.highTexelSize.w)
        return;

    uint2 low_pixel_coord = pixel / 2;
    float2 highResUV = pixel * input.highTexelSize.xy;

    float totalWeights = 0;
    float sum = 0;
    float output = 0;
    const int delta = (KernelSize - 1) / 2;
    float highDepth = input.highDepth.Load(int3(pixel, 0)).r;
    float linearHighResDepth = LinearEyeDepth(highDepth, perScene.cameraZBufferParams);
    for (int x = -delta; x <= delta; x++)
    {
        for (int y = -delta; y <= delta; y++)
        {
            int3 lowResCoord = int3(int2(clamp(low_pixel_coord + int2(x, y), 0, input.lowTexelSize.zw - 1)), 0);
            float2 lowResUV = (lowResCoord.xy + 0.5) * input.lowTexelSize.xy;
            float lowDepth = input.lowdepth.Load(lowResCoord).r;
            float lowColor = input.lowColor.Load(lowResCoord).r;
            float linearLowResDepth = LinearEyeDepth(lowDepth, perScene.cameraZBufferParams);

            float g = Gaussian_v0(length(lowResCoord.xy - pixel), delta);
            float j = Gaussian_v0(linearLowResDepth - linearHighResDepth, input.sigma);
            float weight = g * j;

            output += lowColor * weight;
            totalWeights += weight;
        }
    }

    input.dst[pixel] = output / totalWeights;
}

