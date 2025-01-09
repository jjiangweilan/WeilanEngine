#if CONFIG
name: Game/Fluid/Fog
#endif

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(set = 0, rgba8) uniform image3D imgOutput;
layout(set = 0, rgba8) uniform image3D imgOutput2;
layout(set = 0) uniform Global
{
    float time;
} g;

struct Fog
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
};

ParameterBlock<Fog> prop;

#define USE_PERLIN_NOISE_3D
#define USE_WORLEY_NOISE_3D
#include "Utils/Noise.hlsl"
#include "Utils/Util.hlsl"

void CloudDensity(float3 pos0, float3 pos1, out float4 texOutput0, out float4 texOutput1)
{
    pos0 += float3(0,0,g.time);
    pos1 += float3(0,0,g.time);
    //
    // Initial values
    float4 frequency = prop.worley_freq;
    float perlin = 0;

    float4 amp = prop.perlin0_amp;
    perlin += amp.x * (perlinNoise3D(pos0 * prop.perlin0_freq.x));
    perlin += amp.y * (perlinNoise3D(pos0 * prop.perlin0_freq.y));
    perlin += amp.z * (perlinNoise3D(pos0 * prop.perlin0_freq.z));
    perlin += amp.w * (perlinNoise3D(pos0 * prop.perlin0_freq.w));
    perlin = remap(perlin, prop.perlin0_remap.x, prop.perlin0_remap.y, prop.perlin0_remap.z, prop.perlin0_remap.w);
    //perlin = abs(perlin * 2. - 1.);
    
    float worley = prop.worley_amp.x * (worley3D(frequency.x * pos0));
    float worley0 = (1 - worley3D(frequency.x * pos0));
    float worley1 = (1 - worley3D(frequency.y * pos0));
    float worley2 = (1 - worley3D(frequency.z * pos0));

    float perlinWorley = remap(perlin, worley, 1, 0, 1);
    texOutput0.x = perlinWorley;
    texOutput0.y = worley0;
    texOutput0.z = worley1;
    texOutput0.w = worley2;

    frequency = prop.worley1_freq;
    texOutput1.x = prop.worley1_amp.x * (1 - worley3D(frequency.x * pos1));
    texOutput1.y = prop.worley1_amp.y * (1 - worley3D(frequency.y * pos1));
    texOutput1.z = prop.worley1_amp.z * (1 - worley3D(frequency.z * pos1));
}

[shader("compute")]
void main(uint3 id : SV_GroupThreadID) {
    uint3 coord = id;
    float3 uv1 = (coord / 64.0f + 1/128.0f) * prop.uvScale.xyx;
    float3 uv0 = (coord / 128.0f + 1/256.0f) * prop.uvScale.xyx;
    float4 tex0;
    float4 tex1;
    CloudDensity(uv0, uv1, tex0, tex1);

    if (all(lessThan(coord, ifloat3(64))))
    {
        imageStore(imgOutput2, coord, tex1);
    }
    imageStore(imgOutput, coord, tex0);
}
