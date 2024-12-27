#pragma once

float3 EncodeGBufferNormal(float3 normal)
{
    return normal * 0.5 + 0.5;
}

float3 DecodeGBufferNormal(float3 normal)
{
    return normal * 2 - 1;
}
