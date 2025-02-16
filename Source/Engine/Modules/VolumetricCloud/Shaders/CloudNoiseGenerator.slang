struct NoiseParameter
{
    float perlin_Lacunarity = 2.0;
    float perlin_Gain = 0.5;
    float perlin_Amplitude = 0.5;
    float perlin_Frequency = 1.0;

    float worley_Lacunarity = 2.0;
    float worley_Gain = 0.5;
    float worley_Amplitude = 0.5;
    float worley_Frequency = 1.0;

#if GPU_RESOURCE
    [format("rgba8")]
    RWTexture3D<float4> imgOutput;
#endif
};
ParameterBlock<NoiseParameter> perMaterial;

#define USE_PERLIN_NOISE_3D
#define USE_WORLEY_NOISE_3D
#include "Utils/Noise.hlsl"
#include "Utils/Util.hlsl"

float4 Spherical(float3 uv)
{
    uv = uv - float3(0.5);
    float density = max(0, 1 - length(uv) * 2);
    return density;
}

void CloudDensity(float3 pos0, out float4 texOutput0)
{
    const int octaves = 8;

// perlin
    float noise = 0;
    float lacunarity = perMaterial.perlin_Lacunarity;
    float gain = perMaterial.perlin_Gain;
    float amplitude = perMaterial.perlin_Amplitude;
    float frequency = perMaterial.perlin_Frequency;
    for (int i = 0; i < octaves; i++) {
        noise += amplitude * perlinNoise3DWrap(pos0, frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    texOutput0.x = noise * 0.5 + 0.5;// perlinWorley;

    // worley 0
    noise = 0;
    lacunarity = perMaterial.worley_Lacunarity;
    gain = perMaterial.worley_Gain;
    amplitude = perMaterial.worley_Amplitude;
    frequency = perMaterial.worley_Frequency;
    for (int i = 0; i < octaves; i++) {
        noise += amplitude * worley3D(pos0 + 1024, frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    texOutput0.y = 1 - noise;

    // worley 1
    noise = 0;
    lacunarity = perMaterial.worley_Lacunarity;
    gain = perMaterial.worley_Gain;
    amplitude = perMaterial.worley_Amplitude;
    frequency = perMaterial.worley_Frequency * 2;
    for (int i = 0; i < octaves; i++) {
        noise += amplitude * worley3D(pos0 + 512, frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    texOutput0.z = 1 - noise;

    // worley 2
    noise = 0;
    lacunarity = perMaterial.worley_Lacunarity;
    gain = perMaterial.worley_Gain;
    amplitude = perMaterial.worley_Amplitude;
    frequency = perMaterial.worley_Frequency * 4;
    for (int i = 0; i < octaves; i++) {
        noise += amplitude * worley3D(pos0 + 128, frequency);
        frequency *= lacunarity;
        amplitude *= gain;
    }
    texOutput0.w = 1 - noise;
}

[shader("compute")]
[numthreads(8,8,8)]
void ComputeMain(uint3 id : SV_DispatchThreadID) {
    uint3 coord = id;

    if (all(coord < uint3(128)))
    {
        float3 uv0 = (coord / 128.0f + 1/256.0f);
        float4 tex0;
        // tex0 = Spherical(uv0);
        CloudDensity(uv0, tex0);
        perMaterial.imgOutput[coord] = tex0;
    }
}
