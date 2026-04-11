import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;
import Lighting;

struct GITemporalParameter
{
    float4 rtgiParams;       // (width, height, invWidth, invHeight)
    float4 temporalParams;   // (temporalAlpha, historyValid, 0, 0)

    Texture2D<float4>  rtgiRawTex;
    Texture2D<float4>  historyColorTex;
    Texture2D<float4>  historyMomentsTex; // RG = m1/m2, B = accumCount
    Texture2D<float2>  motionVecTex;
    Texture2D          depthTex;
    Texture2D          historyDepthTex;   // previous-frame depth
    Texture2D          normalTex;
    Texture2D          historyNormalTex;

    RWTexture2D<float4> outAccumulated; // RGB = blended radiance, A = variance
    RWTexture2D<float4> outMoments;     // R = m1, G = m2, B = accumCount

    SamplerState sampler_point_clamp;
    SamplerState sampler_linear_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GITemporalParameter> perMaterial;

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    float width  = perMaterial.rtgiParams.x;
    float height = perMaterial.rtgiParams.y;

    if (pixelCoord.x >= (uint)width || pixelCoord.y >= (uint)height)
        return;

    float2 invSize       = perMaterial.rtgiParams.zw;
    float  temporalAlpha = perMaterial.temporalParams.x;
    bool   historyValid  = perMaterial.temporalParams.y > 0.5;

    float2 uv = (float2(pixelCoord) + 0.5f) * invSize;

    // Current raw radiance
    float3 currentColor = perMaterial.rtgiRawTex[pixelCoord].rgb;
    float  currentLum   = Luminance(currentColor);

    // Reproject via motion vector
    float2 motionVec = perMaterial.motionVecTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).rg;
    float2 prevUV    = uv - motionVec;

    // Current depth — linearized for a meaningful distance threshold
    float depthCurrRaw = perMaterial.depthTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).r;
    float depthCurrLin = LinearEyeDepth(depthCurrRaw, perScene.camera.cameraZBufferParams);

    // Current normal
    float3 currentNormal = DecodeGBufferNormal(perMaterial.normalTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).rgb);

    // Disocclusion: compare current depth against previous-frame depth at reprojected UV
    bool outOfBounds = any(prevUV < 0.0) || any(prevUV > 1.0);
    bool depthFail   = false;
    bool normalFail  = false;
    if (!outOfBounds)
    {
        float depthPrevRaw = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).r;
        float depthPrevLin = LinearEyeDepth(depthPrevRaw, perScene.camera.cameraZBufferParams);
        depthFail = depthCurrLin > 0.01 && (abs(depthCurrLin - depthPrevLin) / (depthCurrLin + 1e-5)) > 0.1;

        float3 historyNormal = DecodeGBufferNormal(perMaterial.historyNormalTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0).rgb);
        normalFail = dot(currentNormal, historyNormal) < 0.9;
    }
    bool disoccluded = outOfBounds || depthFail || normalFail;

    // Per-pixel accumulation count — ramps alpha from 1.0 down to temporalAlpha over N frames
    float4 histMomAll     = perMaterial.historyMomentsTex.SampleLevel(perMaterial.sampler_point_clamp, prevUV, 0);
    float  histAccumCount = (historyValid && !disoccluded) ? histMomAll.b : 0.0;
    float  accumCount     = min(histAccumCount + 1.0, 64.0);

    // Blend weight: alpha=1.0 discards history; ramp-up then settle at temporalAlpha
    float alpha = disoccluded ? 1.0 : max(temporalAlpha, 1.0 / accumCount);

    // Sample history color (bilinear for smoother reprojection)
    float3 histColor = perMaterial.historyColorTex.SampleLevel(perMaterial.sampler_linear_clamp, prevUV, 0).rgb;

    // 3x3 neighborhood clamping for histColor
    float3 minColor = currentColor;
    float3 maxColor = currentColor;
    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            if (i == 0 && j == 0) continue;
            int2 sampleCoord = (int2)pixelCoord + int2(i, j);
            sampleCoord = clamp(sampleCoord, int2(0, 0), int2((int)width - 1, (int)height - 1));
            float3 neighborColor = perMaterial.rtgiRawTex[sampleCoord].rgb;
            minColor = min(minColor, neighborColor);
            maxColor = max(maxColor, neighborColor);
        }
    }
    histColor = clamp(histColor, minColor, maxColor);

    float2 histMom   = histMomAll.rg;

    // Temporally blend color and moments
    float3 blendedColor = lerp(histColor, currentColor, alpha);
    float  m1           = lerp(histMom.r, currentLum, alpha);
    float  m2           = lerp(histMom.g, currentLum * currentLum, alpha);

    // Variance from blended moments
    float variance = max(0.0, m2 - m1 * m1);

    perMaterial.outAccumulated[pixelCoord] = float4(blendedColor, variance);
    perMaterial.outMoments[pixelCoord]     = float4(m1, m2, accumCount, 0.0);
}
