#pragma once

struct ParameterInput
{
    float envMapSize;
    float envMapSizeSqr;
    float totalPixelCount;
    float roughness[6];
};

struct ShaderInput
{
    float4 placeHolder;
#if GPU_RESOURCE
    StructuredBuffer<ParameterInput> input;
    TextureCube srcCubemap; // mipped source cubemap
    SamplerState linearClampSampler;
    RWTexture2D dstFaces[36]; // 6 mips * 6 faces, {faces{mips...}, ...}
#endif
};
