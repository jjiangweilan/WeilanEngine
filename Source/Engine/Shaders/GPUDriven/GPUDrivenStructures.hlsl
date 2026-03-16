#pragma once

struct SceneObjectPositionData
{
    float3 position;

    float3 GetPosition() { return position; }
};

struct SceneObjectAttributeData
{
    float3 normal;
    float4 tangent;
    float2 uv;

    float3 GetNormal() { return normal; }
    float4 GetTangent() { return tangent; }
    float2 GetUV() { return uv; }
};
