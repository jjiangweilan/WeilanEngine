// https://www.shadertoy.com/view/wslyWs
float3 Sky(
    float3 ro,
    float3 rd,
    float3 lightDir,
    bool hasSun,
    float3 skyColor,
    float3 skyHorizonFalloffColor,
    float3 skyHorizonColor,
    float3 skySunColor,
    float3 skySunCoreColor
)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / (rd.y + 0.000001);
    float2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    float3 skyCol = max(skyColor - rd.y * rd.y * 0.5, 0.001);
    skyCol = lerp(skyCol, 0.85 * skyHorizonFalloffColor, pow(1.0 - max(rd.y, 0.0), 4.0));

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    // sun
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * skySunColor * pow(sundot, 5.0);
        sun += 0.25 * skySunCoreColor * pow(sundot, 64.0);
        sun += 0.2 * skySunCoreColor * pow(sundot, 512.0);
        skyCol += sun;
    }

    // horizon
    skyCol = lerp(skyCol, 0.68 * skyHorizonColor, pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol * 0.45;
}
