#pragma once

struct GPUParticle
{
    float4 positionAndFrame;
    float4 rotation;
    float4 scaleAndMode;
    float4 color;
    float4 velocityAndStretch;
    float4 sortData;
};
