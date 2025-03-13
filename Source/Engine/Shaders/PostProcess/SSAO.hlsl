#pragma once

#define SSAO_SAMPLE_COUNT 64
struct SSAOInput
{
    float4 sourceTexelSize;
    float4 samples[SSAO_SAMPLE_COUNT];
    float bias;
    float radius;
    float rangeCheck;
};

