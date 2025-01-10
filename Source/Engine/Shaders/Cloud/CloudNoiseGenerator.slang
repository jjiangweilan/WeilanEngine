struct NoiseParameter
{
    float4 worley_freq;
    float4 worley_amp;
    float4 worley_remap;

    float4 perlin0_freq;
    float4 perlin0_amp;
    float4 perlin0_remap;

    float4 worley1_freq;
    float4 worley1_amp;

    float2 uvScale;
    float p;
    float time;

#if GPU_RESOURCE
    [format("rgba8")]
    RWTexture3D<float4> imgOutput;
    [format("rgba8")]
    RWTexture3D<float4> imgOutput2;
#endif
};
ParameterBlock<NoiseParameter> perMaterial;

#define USE_PERLIN_NOISE_3D
#define USE_WORLEY_NOISE_3D
#include "Utils/Noise.hlsl"
#include "Utils/Util.hlsl"

void CloudDensity(float3 pos0, float3 pos1, out float4 texOutput0, out float4 texOutput1)
{
    pos0 += float3(0,0,perMaterial.time);
    pos1 += float3(0,0,perMaterial.time);
    //
    // Initial values
    float4 frequency = perMaterial.worley_freq;
    float perlin = 0;

    float4 amp = perMaterial.perlin0_amp;
    perlin += amp.x * (perlinNoise3D(pos0 * perMaterial.perlin0_freq.x));
    perlin += amp.y * (perlinNoise3D(pos0 * perMaterial.perlin0_freq.y));
    perlin += amp.z * (perlinNoise3D(pos0 * perMaterial.perlin0_freq.z));
    perlin += amp.w * (perlinNoise3D(pos0 * perMaterial.perlin0_freq.w));
    perlin = remap(perlin, perMaterial.perlin0_remap.x, perMaterial.perlin0_remap.y, perMaterial.perlin0_remap.z, perMaterial.perlin0_remap.w);
    //perlin = abs(perlin * 2. - 1.);
    
    float worley = perMaterial.worley_amp.x * (worley3D(frequency.x * pos0));
    float worley0 = (1 - worley3D(frequency.x * pos0));
    float worley1 = (1 - worley3D(frequency.y * pos0));
    float worley2 = (1 - worley3D(frequency.z * pos0));

    float perlinWorley = remap(perlin, worley, 1, 0, 1);
    texOutput0.x = perlinWorley;
    texOutput0.y = worley0;
    texOutput0.z = worley1;
    texOutput0.w = worley2;

    frequency = perMaterial.worley1_freq;
    texOutput1.x = perMaterial.worley1_amp.x * (1 - worley3D(frequency.x * pos1));
    texOutput1.y = perMaterial.worley1_amp.y * (1 - worley3D(frequency.y * pos1));
    texOutput1.z = perMaterial.worley1_amp.z * (1 - worley3D(frequency.z * pos1));
}

[shader("compute")]
[numthreads(8,8,8)]
void ComputeMain(uint3 id : SV_GroupThreadID) {
    uint3 coord = id;
    float3 uv1 = (coord / 64.0f + 1/128.0f) * perMaterial.uvScale.xyx;
    float3 uv0 = (coord / 128.0f + 1/256.0f) * perMaterial.uvScale.xyx;
    float4 tex0;
    float4 tex1;
    CloudDensity(uv0, uv1, tex0, tex1);

    if (all(coord < uint3(64)))
    {
        perMaterial.imgOutput2[coord] = tex1;
        perMaterial.imgOutput[coord] = tex0;
    }
}
