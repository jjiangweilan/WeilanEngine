#if CONFIG
name: SSILBilateralFilter
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;
import Lighting;

struct SSILBilateralFilterParameter
{
    float4 lowResTexSize;  // (lowWidth, lowHeight, 1/lowWidth, 1/lowHeight)
    float4 highResTexSize; // (highWidth, highHeight, 1/highWidth, 1/highHeight)
    float depthDiffSigma;
    float distanceScale;   // Adaptive plane distance threshold scale
    float2 padding;

    Texture2D<float4> lowColor;
    Texture2D lowDepth;
    Texture2D highDepth;
    Texture2D normalTex;

    [format("rgba16f")]
    RWTexture2D<float4> dst;

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<SSILBilateralFilterParameter> perMaterial;

float3 SSILReconstructWorldPos(float2 uv, float depth)
{
    float4 posWS = mul(perScene.camera.invNDCToWorld, float4(uv * 2.0 - 1.0, depth, 1.0));
    return posWS.xyz / posWS.w;
}

[numthreads(8, 8, 1)]
[shader("compute")]
void Main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= perMaterial.highResTexSize.x || pixel.y >= perMaterial.highResTexSize.y)
        return;

    float2 highResUv = (float2(pixel) + 0.5) * perMaterial.highResTexSize.zw;

    // Center high-res depth
    float centerDepth = perMaterial.highDepth.Load(int3(pixel, 0)).r;
    float centerLinearDepth = LinearEyeDepth(centerDepth, perScene.camera.cameraZBufferParams);

    // Center low-res UV
    uint2 lowResPixel = pixel / 2;
    float2 lowResCentralUv = (float2(lowResPixel) + 0.5) * perMaterial.lowResTexSize.zw;

    // Early-out for sky pixels
    if (centerDepth <= 0.0000001)
    {
        float4 lowColor = perMaterial.lowColor.SampleLevel(perMaterial.sampler_point_clamp, lowResCentralUv, 0);
        perMaterial.dst[pixel] = lowColor;
        return;
    }

    // Reconstruct center world position
    float3 centerPos = SSILReconstructWorldPos(highResUv, centerDepth);

    // Center full-res normal
    float3 centerNormal = normalize(DecodeGBufferNormal(perMaterial.normalTex.Load(int3(pixel, 0)).xyz));

    // Adaptive plane distance threshold (similar to GI)
    float centerPlaneThreshold = max(perMaterial.distanceScale * abs(mul(perScene.camera.view, float4(centerPos, 1.0)).z), 1e-4);

    const int KernelRadius = 1; // 3x3 kernel
    float4 output = 0;
    float totalWeights = 0;

    for (int y = -KernelRadius; y <= KernelRadius; y++)
    {
        for (int x = -KernelRadius; x <= KernelRadius; x++)
        {
            float2 sampleLowResUv = lowResCentralUv + float2(x, y) * perMaterial.lowResTexSize.zw;

            // Clamp UVs
            sampleLowResUv = clamp(sampleLowResUv, float2(0.0, 0.0), float2(1.0, 1.0));

            // Sample low-res SSIL color
            float4 sampleColor = perMaterial.lowColor.SampleLevel(perMaterial.sampler_linear_clamp, sampleLowResUv, 0);

            // Sample low-res depth
            float sampleLowDepth = perMaterial.lowDepth.SampleLevel(perMaterial.sampler_point_clamp, sampleLowResUv, 0).r;

            if (sampleLowDepth <= 0.0000001)
                continue;

            float sampleLinearDepth = LinearEyeDepth(sampleLowDepth, perScene.camera.cameraZBufferParams);

            // Sample normal at the corresponding full-res pixel
            int2 sampleHighResPixel = int2(lowResPixel) + int2(x, y) * 2;
            sampleHighResPixel = clamp(sampleHighResPixel, int2(0, 0), int2(perMaterial.highResTexSize.xy) - 1);
            float3 sampleNormal = normalize(DecodeGBufferNormal(perMaterial.normalTex.Load(int3(sampleHighResPixel, 0)).xyz));

            // Reconstruct sample world position from low-res depth
            float3 samplePos = SSILReconstructWorldPos(sampleLowResUv, sampleLowDepth);

            // 1. Spatial weight: Gaussian over pixel distance in high-res space
            float2 sampleHighResCoord = sampleLowResUv * perMaterial.highResTexSize.xy;
            float spatialWeight = Gaussian_v0(length(sampleHighResCoord - float2(pixel)), 2.0);

            // 2. Depth weight: Gaussian over linear eye depth difference
            float depthWeight = Gaussian_v0(sampleLinearDepth - centerLinearDepth, perMaterial.depthDiffSigma);

            // 3. Normal weight: binary threshold like GI
            float normalWeight = dot(centerNormal, sampleNormal) >= 0.85 ? 1.0 : 0.0;

            // 4. Plane weight: distance from sample position to center tangent plane
            float planeDistance = abs(dot(centerNormal, samplePos - centerPos));
            float planeWeight = saturate(1.0 - planeDistance / centerPlaneThreshold);

            float weight = max(spatialWeight * depthWeight * normalWeight * planeWeight, 0.000001);

            output += sampleColor * weight;
            totalWeights += weight;
        }
    }

    if (totalWeights < 0.000001)
    {
        perMaterial.dst[pixel] = perMaterial.lowColor.SampleLevel(perMaterial.sampler_point_clamp, lowResCentralUv, 0);
    }
    else
    {
        perMaterial.dst[pixel] = output / totalWeights;
    }
}