#pragma once

struct ParameterInput
{
    float envMapSize;
    float envMapSizeSqr;
    float roughness[6];
};

struct ShaderInput
{
#if GPU_RESOURCE
    StructuredBuffer<ParameterInput> input;
    TextureCube srcCubemap; // mipped source cubemap
    SamplerState linearClampSampler;
    RWTexture2D dstFaces[30]; // 6 mips * 5 faces, {mips{faces...}, ...}
#endif
};
