import WeilanEngine;
import Library.CommonUtils;

struct VariancePrefilterParameter
{
    float4 rtgiParams;   // (width, height, invWidth, invHeight)

    Texture2D<float4>   inTex;   // RGB = radiance, A = variance
    RWTexture2D<float4> outTex;  // RGB = radiance, A = filtered variance
};

[Material]
ParameterBlock<VariancePrefilterParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    float width  = perMaterial.rtgiParams.x;
    float height = perMaterial.rtgiParams.y;

    if (pixelCoord.x >= (uint)width || pixelCoord.y >= (uint)height)
        return;

    float3 centerColor = perMaterial.inTex.Load(int3(pixelCoord, 0)).rgb;

    // 3x3 Gaussian kernel
    // 1 2 1
    // 2 4 2
    // 1 2 1  / 16

    float kernel[3] = { 1.0 / 4.0, 2.0 / 4.0, 1.0 / 4.0 };

    float varSum = 0.0;
    float weightSum = 0.0;

    for (int j = -1; j <= 1; j++)
    {
        for (int i = -1; i <= 1; i++)
        {
            int2 sampleCoord = clamp(int2(pixelCoord) + int2(i, j), int2(0, 0), int2(width - 1, height - 1));
            float sampleVar = perMaterial.inTex.Load(int3(sampleCoord, 0)).a;

            float weight = kernel[i + 1] * kernel[j + 1];
            varSum += sampleVar * weight;
            weightSum += weight;
        }
    }

    float filteredVar = varSum / max(weightSum, 1e-6);
    perMaterial.outTex[pixelCoord] = float4(centerColor, filteredVar);
}
