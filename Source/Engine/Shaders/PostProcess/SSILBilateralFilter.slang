#if CONFIG
name: SSILBilateralFilter
#endif

import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

struct SSILBilateralFilterParameter
{
    float4 lowResTexSize;  // (lowWidth, lowHeight, 1/lowWidth, 1/lowHeight)
    float4 highResTexSize; // (highWidth, highHeight, 1/highWidth, 1/highHeight)
    float depthDiffSigma;
    float distanceScale;
    float2 padding;

    Texture2D<float4> lowColor;
    Texture2D<uint4> lowGeometryTex;   // half-res packed geometry (SSILGeometry)
    Texture2D highDepth;               // full-res depth mip 0
    Texture2D<uint> highSmoothNormal;  // full-res packed smooth normal (SSILSmoothNormal)

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

    // Center high-res depth and smooth normal
    float centerDepth = perMaterial.highDepth.Load(int3(pixel, 0)).r;
    if (centerDepth <= 0.0000001)
    {
        uint2 halfPixel = min(pixel / 2, uint2(perMaterial.lowResTexSize.xy) - 1);
        float2 lowResUv = (float2(halfPixel) + 0.5) * perMaterial.lowResTexSize.zw;
        float4 lowColor = perMaterial.lowColor.SampleLevel(perMaterial.sampler_point_clamp, lowResUv, 0);
        perMaterial.dst[pixel] = lowColor;
        return;
    }

    float centerLinearDepth = LinearEyeDepth(centerDepth, perScene.camera.cameraZBufferParams);
    float3 centerPos = SSILReconstructWorldPos(highResUv, centerDepth);
    uint centerNormalPacked = perMaterial.highSmoothNormal.Load(int3(pixel, 0)).x;
    float3 centerNormal = UnpackNormalOctahedral16(centerNormalPacked);

    float centerPlaneThreshold = max(perMaterial.distanceScale * abs(mul(perScene.camera.view, float4(centerPos, 1.0)).z), 1e-4);

    const int KernelRadius = 1; // 3x3 kernel
    uint2 lowResPixel = pixel / 2;
    float2 lowResCentralUv = (float2(lowResPixel) + 0.5) * perMaterial.lowResTexSize.zw;

    float4 output = 0;
    float totalWeights = 0;

    for (int y = -KernelRadius; y <= KernelRadius; y++)
    {
        for (int x = -KernelRadius; x <= KernelRadius; x++)
        {
            float2 sampleLowResUv = lowResCentralUv + float2(x, y) * perMaterial.lowResTexSize.zw;
            sampleLowResUv = clamp(sampleLowResUv, float2(0.0, 0.0), float2(1.0, 1.0));

            float4 sampleColor = perMaterial.lowColor.SampleLevel(perMaterial.sampler_linear_clamp, sampleLowResUv, 0);

            // Decode half-res geometry for this sample
            uint2 sampleLowResCoord = uint2(clamp(int2(lowResPixel) + int2(x, y), int2(0, 0), int2(perMaterial.lowResTexSize.xy) - 1));
            uint4 samplePacked = perMaterial.lowGeometryTex.Load(int3(sampleLowResCoord, 0));
            float sampleDepth = asfloat(samplePacked.x);

            if (sampleDepth <= 0.0000001)
                continue;

            float sampleLinearDepth = LinearEyeDepth(sampleDepth, perScene.camera.cameraZBufferParams);
            float3 sampleNormal = UnpackNormalOctahedral16(samplePacked.y);

            float2 sampleUv = (float2(samplePacked.zw) + 0.5) * perMaterial.highResTexSize.zw;
            float3 samplePos = SSILReconstructWorldPos(sampleUv, sampleDepth);

            // 1. Spatial weight
            float2 sampleHighResCoord = sampleLowResUv * perMaterial.highResTexSize.xy;
            float spatialWeight = Gaussian_v0(length(sampleHighResCoord - float2(pixel)), 2.0);

            // 2. Depth weight
            float depthWeight = Gaussian_v0(sampleLinearDepth - centerLinearDepth, perMaterial.depthDiffSigma);

            // 3. Normal weight
            float normalWeight = dot(centerNormal, sampleNormal) >= 0.85 ? 1.0 : 0.0;

            // 4. Plane weight
            float planeDistance = abs(dot(centerNormal, samplePos - centerPos));
            float planeWeight = saturate(1.0 - planeDistance / centerPlaneThreshold);

            float weight = spatialWeight * depthWeight * normalWeight * planeWeight;
            if (weight <= 0.0)
                continue;

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
