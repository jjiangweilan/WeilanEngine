import WeilanEngine;
import Library.CommonUtils;
import Library.PerScene;

struct TAAParameter
{
    float4 rtSize;
    float historyWeight;
    float varianceClipGamma;
    float historyValid;

    Texture2D<float4> currentColorTex;
    Texture2D<float2> motionVectorTex;
    Texture2D<float2> currentDepthTex;
    Texture2D<float4> historyColorTex;
    Texture2D<float> historyDepthTex;
    RWTexture2D<float4> outHistoryTex;
    RWTexture2D<float> outHistoryDepthTex;

    SamplerState sampler_linear_clamp;
    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<TAAParameter> perMaterial;

float3 RGBToYCoCg(float3 color)
{
    float co = color.r - color.b;
    float t = color.b + co * 0.5;
    float cg = color.g - t;
    return float3(t + cg * 0.5, co, cg);
}

float3 YCoCgToRGB(float3 color)
{
    float t = color.x - color.z * 0.5;
    float g = color.z + t;
    float b = t - color.y * 0.5;
    return float3(color.y + b, g, b);
}

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 inverseViewProjection)
{
    float4 position = mul(inverseViewProjection, float4(uv * 2.0 - 1.0, depth, 1.0));
    return position.xyz / position.w;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    uint2 size = uint2(perMaterial.rtSize.xy);
    if (any(coord >= size))
        return;

    float2 uv = (float2(coord) + 0.5) * perMaterial.rtSize.zw;
    float4 currentColor = perMaterial.currentColorTex.Load(int3(coord, 0));
    float currentDepth = perMaterial.currentDepthTex.Load(int3(coord, 0)).x;
    perMaterial.outHistoryDepthTex[coord] = currentDepth;

    float3 mean = 0.0;
    float3 secondMoment = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 sampleCoord = clamp(int2(coord) + int2(x, y), int2(0), int2(size) - 1);
            float3 sampleRgb = perMaterial.currentColorTex.Load(int3(sampleCoord, 0)).rgb;
            float3 sampleYCoCg = RGBToYCoCg(sampleRgb);
            mean += sampleYCoCg;
            secondMoment += sampleYCoCg * sampleYCoCg;
        }
    }
    mean /= 9.0;
    secondMoment /= 9.0;

    float3 resolved = currentColor.rgb;
    if (perMaterial.historyValid > 0.5)
    {
        float2 motion = perMaterial.motionVectorTex.SampleLevel(perMaterial.sampler_point_clamp, uv, 0).xy;
        float2 previousUv = uv - motion;
        bool validHistory = all(previousUv >= 0.0) && all(previousUv <= 1.0);

        if (validHistory)
        {
            float historyDepth = perMaterial.historyDepthTex.SampleLevel(perMaterial.sampler_point_clamp, previousUv, 0).x;
            bool currentGeometry = currentDepth > 0.0000001;
            bool historyGeometry = historyDepth > 0.0000001;
            validHistory = currentGeometry == historyGeometry;

            if (validHistory && currentGeometry)
            {
                float3 currentPosition = ReconstructWorldPosition(uv, currentDepth, perScene.camera.invNDCToWorld);
                float3 historyPosition = ReconstructWorldPosition(
                    previousUv,
                    historyDepth,
                    perScene.camera.invPreviousViewProjection
                );
                float currentViewDepth = abs(mul(perScene.camera.view, float4(currentPosition, 1.0)).z);
                float positionTolerance = max(currentViewDepth * 0.02, 0.001);
                validHistory = length(historyPosition - currentPosition) <= positionTolerance;
            }
        }

        if (validHistory)
        {
            float3 history = perMaterial.historyColorTex.SampleLevel(
                perMaterial.sampler_linear_clamp,
                previousUv,
                0
            ).rgb;
            float3 variance = max(secondMoment - mean * mean, 0.0);
            float3 sigma = sqrt(variance);
            float gamma = max(perMaterial.varianceClipGamma, 0.0);
            float3 historyYCoCg = clamp(RGBToYCoCg(history), mean - gamma * sigma, mean + gamma * sigma);
            history = YCoCgToRGB(historyYCoCg);
            resolved = lerp(currentColor.rgb, history, saturate(perMaterial.historyWeight));
        }
    }

    resolved = max(resolved, 0.0);
    perMaterial.outHistoryTex[coord] = float4(resolved, currentColor.a);
}
