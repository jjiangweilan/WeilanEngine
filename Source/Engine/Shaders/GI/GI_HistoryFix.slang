import WeilanEngine;
import Library.PerScene;
import GI.GI_Common;

struct GIHistoryFixParameter
{
    float4 rtSize; // (fullWidth, fullHeight, 1/fullWidth, 1/fullHeight)

    Texture2D hierarchyDepth;
    Texture2D<float4> inSH0Tex;
    Texture2D<float4> inSH1Tex;
    Texture2D<float4> inSH2Tex;

    [format("rgba16f")]
    RWTexture2D<float4> outSH0Mips[5];
    [format("rgba16f")]
    RWTexture2D<float4> outSH1Mips[5];
    [format("rgba16f")]
    RWTexture2D<float4> outSH2Mips[5];
    [format("rgba16f")]
    RWTexture2D<float4> outViewZMips[5];

    SamplerState sampler_point_clamp;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<GIHistoryFixParameter> perMaterial;

uint2 GetMipSize(uint2 baseSize, uint mip)
{
    uint scale = 1u << mip;
    return uint2(
        max((baseSize.x + scale - 1u) / scale, 1u),
        max((baseSize.y + scale - 1u) / scale, 1u)
    );
}

bool LoadProbeInput(
    uint2 probeCoord,
    uint2 probeRtSize,
    out float4 sh0,
    out float4 sh1,
    out float4 sh2,
    out float viewZ)
{
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
    {
        sh0 = 0;
        sh1 = 0;
        sh2 = 0;
        viewZ = 0.0;
        return false;
    }

    uint2 probeFullResCoord = GetSHProbeSourceCoord(probeCoord, perMaterial.rtSize.xy);
    float2 probeUv = GetFullResUv(probeFullResCoord, perMaterial.rtSize);
    float depth = perMaterial.hierarchyDepth.SampleLevel(perMaterial.sampler_point_clamp, probeUv, 0).x;
    if (depth <= 0.0000001)
    {
        sh0 = 0;
        sh1 = 0;
        sh2 = 0;
        viewZ = 0.0;
        return false;
    }

    sh0 = perMaterial.inSH0Tex.Load(int3(probeCoord, 0));
    sh1 = perMaterial.inSH1Tex.Load(int3(probeCoord, 0));
    sh2 = perMaterial.inSH2Tex.Load(int3(probeCoord, 0));

    float3 positionWS = ReconstructWorldPos(perScene, probeUv, depth);
    viewZ = GetViewDepth(perScene, positionWS);
    return true;
}

void GatherProbeBlock(
    uint2 blockStart,
    uint scale,
    uint2 probeRtSize,
    out float4 sh0Sum,
    out float4 sh1Sum,
    out float4 sh2Sum,
    out float viewZSum,
    out float validCount)
{
    sh0Sum = 0;
    sh1Sum = 0;
    sh2Sum = 0;
    viewZSum = 0.0;
    validCount = 0.0;

    for (uint y = 0u; y < scale; ++y)
    {
        for (uint x = 0u; x < scale; ++x)
        {
            uint2 sampleCoord = blockStart + uint2(x, y);

            float4 sampleSH0;
            float4 sampleSH1;
            float4 sampleSH2;
            float sampleViewZ = 0.0;
            if (!LoadProbeInput(sampleCoord, probeRtSize, sampleSH0, sampleSH1, sampleSH2, sampleViewZ))
                continue;

            sh0Sum += sampleSH0;
            sh1Sum += sampleSH1;
            sh2Sum += sampleSH2;
            viewZSum += sampleViewZ;
            validCount += 1.0;
        }
    }
}

void WriteMip(uint mip, uint2 coord, uint2 probeRtSize)
{
    uint2 mipSize = GetMipSize(probeRtSize, mip);
    if (coord.x >= mipSize.x || coord.y >= mipSize.y)
        return;

    uint scale = 1u << mip;
    uint2 blockStart = coord * scale;

    float4 sh0Sum;
    float4 sh1Sum;
    float4 sh2Sum;
    float viewZSum = 0.0;
    float validCount = 0.0;
    GatherProbeBlock(blockStart, scale, probeRtSize, sh0Sum, sh1Sum, sh2Sum, viewZSum, validCount);

    float invValid = validCount > 0.0 ? (1.0 / validCount) : 0.0;
    float4 outSH0 = sh0Sum * invValid;
    float4 outSH1 = sh1Sum * invValid;
    float4 outSH2 = sh2Sum * invValid;
    float4 outViewZ = validCount > 0.0 ? float4(viewZSum * invValid, 0.0, 0.0, 0.0) : 0.0;

    if (mip == 0u)
    {
        perMaterial.outSH0Mips[0][coord] = outSH0;
        perMaterial.outSH1Mips[0][coord] = outSH1;
        perMaterial.outSH2Mips[0][coord] = outSH2;
        perMaterial.outViewZMips[0][coord] = outViewZ;
    }
    else if (mip == 1u)
    {
        perMaterial.outSH0Mips[1][coord] = outSH0;
        perMaterial.outSH1Mips[1][coord] = outSH1;
        perMaterial.outSH2Mips[1][coord] = outSH2;
        perMaterial.outViewZMips[1][coord] = outViewZ;
    }
    else if (mip == 2u)
    {
        perMaterial.outSH0Mips[2][coord] = outSH0;
        perMaterial.outSH1Mips[2][coord] = outSH1;
        perMaterial.outSH2Mips[2][coord] = outSH2;
        perMaterial.outViewZMips[2][coord] = outViewZ;
    }
    else if (mip == 3u)
    {
        perMaterial.outSH0Mips[3][coord] = outSH0;
        perMaterial.outSH1Mips[3][coord] = outSH1;
        perMaterial.outSH2Mips[3][coord] = outSH2;
        perMaterial.outViewZMips[3][coord] = outViewZ;
    }
    else
    {
        perMaterial.outSH0Mips[4][coord] = outSH0;
        perMaterial.outSH1Mips[4][coord] = outSH1;
        perMaterial.outSH2Mips[4][coord] = outSH2;
        perMaterial.outViewZMips[4][coord] = outViewZ;
    }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 probeCoord = dispatchThreadID.xy;
    uint2 probeRtSize = GetSHProbeResolution(perMaterial.rtSize.xy);
    if (probeCoord.x >= probeRtSize.x || probeCoord.y >= probeRtSize.y)
        return;

    WriteMip(0u, probeCoord, probeRtSize);

    if ((probeCoord.x & 1u) == 0u && (probeCoord.y & 1u) == 0u)
        WriteMip(1u, probeCoord >> 1u, probeRtSize);

    if ((probeCoord.x & 3u) == 0u && (probeCoord.y & 3u) == 0u)
        WriteMip(2u, probeCoord >> 2u, probeRtSize);

    if ((probeCoord.x & 7u) == 0u && (probeCoord.y & 7u) == 0u)
        WriteMip(3u, probeCoord >> 3u, probeRtSize);

    if ((probeCoord.x & 15u) == 0u && (probeCoord.y & 15u) == 0u)
        WriteMip(4u, probeCoord >> 4u, probeRtSize);
}
