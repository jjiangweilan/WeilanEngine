static const float M_PI = 3.14159265359;

float RadicalInverse(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse(i));
}

float3 GetCubemapDirection(float2 uv, int faceIndex)
{
    uv = float2(2.0) * uv - float2(1.0); // Normalize UV to range [-1, 1]

    if (faceIndex == 0)
        return float3(1.0, -uv.y, -uv.x); // +X
    if (faceIndex == 1)
        return float3(-1.0, -uv.y, uv.x); // -X
    if (faceIndex == 2)
        return float3(uv.x, 1.0, uv.y); // +Y
    if (faceIndex == 3)
        return float3(uv.x, -1.0, -uv.y); // -Y
    if (faceIndex == 4)
        return float3(uv.x, -uv.y, 1.0); // +Z
    if (faceIndex == 5)
        return float3(-uv.x, -uv.y, -1.0); // -Z

    return float3(0.0);
}
