float SRGBToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : pow((value + 0.055f) / 1.055f, 2.4f);
}

float LinearToSRGB(float value)
{
    return value <= 0.0031308f ? value * 12.92f : 1.055f * pow(value, 1.0f / 2.4f) - 0.055f;
}

float3 SRGBToLinear(float3 value)
{
    return float3(SRGBToLinear(value.r), SRGBToLinear(value.g), SRGBToLinear(value.b));
}

float4 SRGBToLinear(float4 value)
{
    return float4(SRGBToLinear(value.rgb), value.a);
}

float3 LinearToSRGB(float3 value)
{
    return float3(LinearToSRGB(value.r), LinearToSRGB(value.g), LinearToSRGB(value.b));
}

float4 LinearToSRGB(float4 value)
{
    return float4(LinearToSRGB(value.rgb), value.a);
}
