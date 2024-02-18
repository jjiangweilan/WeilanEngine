#ifndef NOISE_INCLUDED
#define NOISE_INCLUDED

// 3d perlin noise
// from https://www.shadertoy.com/view/XlKyRw
#if defined(USE_PERLIN_NOISE_3D) && defined(NOISE_WHITE_NOISE_TEX)
float fade(float t) 
{
    return t*t*t*(t*(6.0*t-15.0)+10.0); 
}

float hash13(vec3 pos)
{
    vec2 uv = pos.xy + pos.z;
    return texture(NOISE_WHITE_NOISE_TEX, (uv+ 0.5)/256.0).x;
}

float grad3D(float hash, vec3 pos) 
{
    int h = int(1e4*hash) & 15;
    float u = h<8 ? pos.x : pos.y,
          v = h<4 ? pos.y : h==12||h==14 ? pos.x : pos.z;
    return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

float perlinNoise3D(vec3 pos)
{
    vec3 pi = floor(pos); 
    vec3 pf = pos - pi;

    float u = fade(pf.x);
    float v = fade(pf.y);
    float w = fade(pf.z);

    return mix( mix( mix( grad3D(hash13(pi + vec3(0, 0, 0)), pf - vec3(0, 0, 0)),
                    grad3D(hash13(pi + vec3(1, 0, 0)), pf - vec3(1, 0, 0)), u ),
                mix( grad3D(hash13(pi + vec3(0, 1, 0)), pf - vec3(0, 1, 0)), 
                    grad3D(hash13(pi + vec3(1, 1, 0)), pf - vec3(1, 1, 0)), u ), v ),
            mix( mix( grad3D(hash13(pi + vec3(0, 0, 1)), pf - vec3(0, 0, 1)), 
                    grad3D(hash13(pi + vec3(1, 0, 1)), pf - vec3(1, 0, 1)), u ),
                mix( grad3D(hash13(pi + vec3(0, 1, 1)), pf - vec3(0, 1, 1)), 
                    grad3D(hash13(pi + vec3(1, 1, 1)), pf - vec3(1, 1, 1)), u ), v ), w );
}
#endif

#endif
