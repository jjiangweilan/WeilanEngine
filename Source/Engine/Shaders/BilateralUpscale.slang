#if CONFIG
name: BilateralUpscale
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;
#include "BilateralUpscaleInput.hlsl"

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<Input> input;

[numthreads(8,8,1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= input.highResTexSize.x || pixel.y >= input.highResTexSize.y)
        return;

    float totalWeights = 0;
    float output = 0;

    // prepare pixel data
    float highDepth = input.highDepth.Load(int3(pixel, 0)).r;
    float linearHighResDepth = LinearEyeDepth(highDepth, perScene.camera.cameraZBufferParams);

    // Calculate central low res uv
    uint2 lowResPixel = pixel / 2;
    float2 lowResCentralUv = (lowResPixel + 0.5f) * (input.lowResTexSize.zw);

    const int KernelSize = (int)input.kernelScale;
    for (int x = -KernelSize; x <= KernelSize; x++)
    {
        for (int y = -KernelSize; y <= KernelSize; y++)
        {
            float2 lowResUv = lowResCentralUv + float2(x, y) * input.lowResTexSize.zw;
            float lowDepth = input.lowDepth.SampleLevel(input.sampler_point_clamp, lowResUv, 0).r;
            float lowColor = input.lowColor.SampleLevel(input.sampler_point_clamp, lowResUv, 0).r;
            float linearLowResDepth = LinearEyeDepth(lowDepth, perScene.camera.cameraZBufferParams);

            float2 highResPixel = lowResUv * input.highResTexSize.xy;
            float g = Gaussian_v0(length(highResPixel - pixel), input.integerCoordSigma);
            float j = Gaussian_v0(linearLowResDepth - linearHighResDepth, input.depthDiffSigma);
            float weight = g * j;

            output += lowColor * weight;
            totalWeights += weight;
        }
    }

    input.dst[pixel] = output / totalWeights;
}

