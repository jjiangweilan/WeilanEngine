#if CONFIG
name: BilateralUpscale
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;
#include "BilateralUpscaleInput.hlsl"

[[Global]]
ParameterBlock<PerScene> perScene;
[[Material]]
ParameterBlock<Input> input;

void AccumulateBilateralWeight(
    float2 lowResUv,
    uint2 highResPixel,
    float linearHighResDepth,
    int kernelSize,
    inout float4 output,
    inout float totalWeights)
{
    float lowDepth = input.lowDepth.SampleLevel(input.sampler_point_clamp, lowResUv, 1).y;
    float4 lowColor = input.lowColor.SampleLevel(input.sampler_linear_clamp, lowResUv, 0);
    float linearLowResDepth = LinearEyeDepth(lowDepth, perScene.camera.cameraZBufferParams);

    float2 currHighResPixel = lowResUv * input.highResTexSize.xy;

    const float integerCoordSigma = 2.0f;//input.integerCoordSigma;
    float g = Gaussian_v0(length(currHighResPixel - float2(highResPixel)), integerCoordSigma);
    float j = Gaussian_v0(linearLowResDepth - linearHighResDepth, input.depthDiffSigma);
    float weight = max(g * j, 0.000001);

    output += lowColor * weight;
    totalWeights += weight;
}

[numthreads(8,8,1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= input.highResTexSize.x || pixel.y >= input.highResTexSize.y)
        return;

    float totalWeights = 0;
    float4 output = 0;

    // prepare pixel data
    float highDepth = input.highDepth.Load(int3(pixel, 0)).r;
    float linearHighResDepth = LinearEyeDepth(highDepth, perScene.camera.cameraZBufferParams);

    // Calculate central low res uv
    uint2 lowResPixel = pixel / 2;
    float2 lowResCentralUv = (lowResPixel + 0.5f) * (input.lowResTexSize.zw);

    const int KernelSize = 1;//(int)input.kernelScale;

    for (int x = -KernelSize; x <= KernelSize; x++)
    {
        for (int y = -KernelSize; y <= KernelSize; y++)
        {
            float2 lowResUv = lowResCentralUv + float2(x, y) * input.lowResTexSize.zw;
            AccumulateBilateralWeight(lowResUv, pixel, linearHighResDepth, KernelSize, output, totalWeights);
        }
    }

    input.dst[pixel] = output / totalWeights;
}

