import WeilanEngine;

struct TAASharpenParameter
{
    float4 rtSize;
    float sharpness;

    Texture2D<float4> inputTex;
    RWTexture2D<float4> outputTex;
};

[Material]
ParameterBlock<TAASharpenParameter> perMaterial;

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 coord = dispatchThreadID.xy;
    uint2 size = uint2(perMaterial.rtSize.xy);
    if (any(coord >= size))
        return;

    float4 center = perMaterial.inputTex.Load(int3(coord, 0));
    float3 neighborhood = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 sampleCoord = clamp(int2(coord) + int2(x, y), int2(0), int2(size) - 1);
            neighborhood += perMaterial.inputTex.Load(int3(sampleCoord, 0)).rgb;
        }
    }

    neighborhood /= 9.0;
    float3 sharpened = center.rgb + saturate(perMaterial.sharpness) * (center.rgb - neighborhood);
    perMaterial.outputTex[coord] = float4(max(sharpened, 0.0), center.a);
}
