#pragma once

float remap(float value, float originMin, float originMax, float newMin, float newMax) {
    return newMin + (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

float remap(float value, float4 from2to2) {
    return from2to2.z + (value - from2to2.x) / (from2to2.y - from2to2.x) * (from2to2.w - from2to2.z);
}

float3 remap(float3 value, float4 from2to2) {
    return float3(from2to2.z) + (value - float3(from2to2.x)) / (float3(from2to2.y) - float3(from2to2.x)) * (float3(from2to2.w) - float3(from2to2.z));
}

// rotation
float2x2 Rotate2D(float angle){
    return float2x2(cos(angle), -sin(angle), sin(angle), cos(angle));
}
