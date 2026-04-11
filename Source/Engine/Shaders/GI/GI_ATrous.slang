import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;

// B3-spline kernel weights for à-trous filter: [3/8, 1/4, 1/16] for offsets [0, 1, 2]
static const float kKernelWeights[3] = { 3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0 };

struct GIATrousPushConstant
{
    float4 rtgiParams;   // (width, height, invWidth, invHeight)
    float4 filterParams; // (stepSize, sigmaDepth, sigmaNormal, sigmaLuminance)
};

[vk_push_constant]
GIATrousPushConstant pconst;

struct GIATrousParameter
{
    Texture2D<float4>   inTex;   // RGB = radiance, A = variance (read via Load)
    Texture2D           depthTex;
    Texture2D           normalTex;
    [format("rgba16f")]
    RWTexture2D<float4> outTex;  // RGB = filtered radiance, A = filtered variance

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIATrousParameter> perMaterial;

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    float width  = pconst.rtgiParams.x;
    float height = pconst.rtgiParams.y;

    if (pixelCoord.x >= (uint)width || pixelCoord.y >= (uint)height)
        return;

    float2 invSize    = pconst.rtgiParams.zw;
    int    stepSize   = (int)pconst.filterParams.x;
    float  sigmaD     = pconst.filterParams.y;
    float  sigmaN     = pconst.filterParams.z;
    float  sigmaL     = pconst.filterParams.w;

    float2 uvCenter = (float2(pixelCoord) + 0.5f) * invSize;

    float4 centerSample = perMaterial.inTex.Load(int3(pixelCoord, 0));
    float3 centerColor  = centerSample.rgb;
    float  centerVar    = centerSample.a;
    float  centerLum    = Luminance(centerColor);

    float  centerDepth  = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uvCenter, 0).r;
    float3 centerNormal = normalize(DecodeGBufferNormal(
        perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uvCenter, 0).xyz));

    // Standard deviation of luminance used to modulate the luminance stopping weight
    float lumStdDev = sqrt(max(0.0, centerVar));

    float3 colorSum  = float3(0.0, 0.0, 0.0);
    float  varSum    = 0.0;
    float  weightSum = 0.0;

    int maxCoordX = (int)width  - 1;
    int maxCoordY = (int)height - 1;

    for (int j = -2; j <= 2; j++)
    {
        for (int i = -2; i <= 2; i++)
        {
            int2 sampleCoord = clamp(
                int2(pixelCoord) + int2(i, j) * stepSize,
                int2(0, 0),
                int2(maxCoordX, maxCoordY)
            );

            float2 sampleUV = (float2(sampleCoord) + 0.5f) * invSize;

            float4 sampleData   = perMaterial.inTex.Load(int3(sampleCoord, 0));
            float3 sampleColor  = sampleData.rgb;
            float  sampleVar    = sampleData.a;
            float  sampleLum    = Luminance(sampleColor);

            float  sampleDepth  = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, sampleUV, 0).r;
            float3 sampleNormal = normalize(DecodeGBufferNormal(
                perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, sampleUV, 0).xyz));

            // B3-spline kernel weight
            float kw = kKernelWeights[abs(i)] * kKernelWeights[abs(j)];

            // Depth edge-stopping: relative depth difference
            float depthDiff = abs(centerDepth - sampleDepth) / (sigmaD * max(centerDepth, 1e-4) + 1e-5);
            float wDepth    = exp(-depthDiff);

            // Normal edge-stopping: raised cosine, made more forgiving on curved surfaces
            float wNormal = pow(max(0.0, dot(centerNormal, sampleNormal)), sigmaN * 0.5);

            // Luminance edge-stopping guided by variance
            float lumDiff = abs(centerLum - sampleLum);
            float wLum    = exp(-lumDiff / (sigmaL * lumStdDev + 1e-4));

            float w = kw * wDepth * wNormal * wLum;

            colorSum  += sampleColor * w;
            varSum    += sampleVar * w * w;
            weightSum += w;
        }
    }

    float invWeight    = 1.0 / max(weightSum, 1e-4);
    float3 filteredColor = colorSum * invWeight;
    // Variance estimate propagated through the filter (squared kernel normalization)
    float  filteredVar   = varSum * invWeight * invWeight;

    perMaterial.outTex[pixelCoord] = float4(filteredColor, filteredVar);
}
