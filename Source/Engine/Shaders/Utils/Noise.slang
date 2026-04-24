#pragma once

// seems not working on RTX gpu, maybe because of sin?
float rand(float2 co){
    return frac(sin(dot(co.xy ,float2(12.9898,78.233))) * 43758.5453);
}

float3 hash3(uint3 x)
{
    const uint k = 1103515245U;  // GLIB C

    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    x = ((x>>8U)^x.yzx)*k;
    
    return float3(x)*(1.0/float(0xffffffffU));
}

// iq hash
// https://www.shadertoy.com/view/4tXyWN
float hash1( uint2 x )
{
    uint2 q = 1103515245U * ( (x>>1U) ^ (x.yx   ) );
    uint  n = 1103515245U * ( (q.x  ) ^ (q.y>>3U) );
    return float(n) * (1.0/float(0xffffffffU));
}


// 3d perlin noise
// modified from https://www.shadertoy.com/view/XlKyRw
// define NOISE_WHITE_NOISE_TEX to a sampler2D if you don't want to generate random number by code
float fade(float t) 
{
    return t*t*t*(t*(6.0*t-15.0) + 10.0f); 
}

float hash13(float3 pos)
{
    float2 uv = pos.xy + pos.z * 1341.5331;
    return hash1(asuint(uv));
// #if defined(NOISE_WHITE_NOISE_TEX)
//     return texture(NOISE_WHITE_NOISE_TEX, (uv+ 0.5)/256.0).x;
// #else
// #endif
}

float grad3D(float hash, float3 pos) 
{
    int h = int(1e4*hash) & 15;
    float u = h<8 ? pos.x : pos.y,
          v = h<4 ? pos.y : h==12||h==14 ? pos.x : pos.z;
    return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

float perlinNoise3D(float3 pos)
{
    float3 pi = floor(pos); 
    float3 pf = pos - pi;

    float u = fade(pf.x);
    float v = fade(pf.y);
    float w = fade(pf.z);

    return lerp( lerp( lerp( grad3D(hash13(pi + float3(0, 0, 0)), pf - float3(0, 0, 0)),
                    grad3D(hash13(pi + float3(1, 0, 0)), pf - float3(1, 0, 0)), u ),
                lerp( grad3D(hash13(pi + float3(0, 1, 0)), pf - float3(0, 1, 0)), 
                    grad3D(hash13(pi + float3(1, 1, 0)), pf - float3(1, 1, 0)), u ), v ),
            lerp( lerp( grad3D(hash13(pi + float3(0, 0, 1)), pf - float3(0, 0, 1)), 
                    grad3D(hash13(pi + float3(1, 0, 1)), pf - float3(1, 0, 1)), u ),
                lerp( grad3D(hash13(pi + float3(0, 1, 1)), pf - float3(0, 1, 1)), 
                    grad3D(hash13(pi + float3(1, 1, 1)), pf - float3(1, 1, 1)), u ), v ), w );
}

float3 mod(float3 x, float3 y)
{
    return x - y * floor(x/y);
}

float perlinNoise3DWrap(float3 pos, float period)
{
    pos *= period;
    float3 pi = floor(pos); 
    float3 pf = pos - pi;

    float u = fade(pf.x);
    float v = fade(pf.y);
    float w = fade(pf.z);

    float grad000 = grad3D(hash13(mod(pi + float3(0, 0, 0), period)), pf - float3(0, 0, 0));
    float grad100 = grad3D(hash13(mod(pi + float3(1, 0, 0), period)), pf - float3(1, 0, 0));
    float grad010 = grad3D(hash13(mod(pi + float3(0, 1, 0), period)), pf - float3(0, 1, 0));
    float grad110 = grad3D(hash13(mod(pi + float3(1, 1, 0), period)), pf - float3(1, 1, 0));
    float grad001 = grad3D(hash13(mod(pi + float3(0, 0, 1), period)), pf - float3(0, 0, 1));
    float grad101 = grad3D(hash13(mod(pi + float3(1, 0, 1), period)), pf - float3(1, 0, 1));
    float grad011 = grad3D(hash13(mod(pi + float3(0, 1, 1), period)), pf - float3(0, 1, 1));
    float grad111 = grad3D(hash13(mod(pi + float3(1, 1, 1), period)), pf - float3(1, 1, 1));

    return lerp(
            lerp(lerp(grad000,grad100,u),lerp(grad010, grad110,u),v),
            lerp(lerp(grad001,grad101,u),lerp(grad011, grad111,u),v),
            w);
}

// worley noise
// from https://www.shadertoy.com/view/MstGRl
#if defined(USE_WORLEY_NOISE) && defined(NOISE_WORLEY_NOISE_NUM_CELLS)

// Returns the point in a given cell
float2 get_cell_point(ifloat2 cell) {
    float2 cell_base = float2(cell) / NOISE_WORLEY_NOISE_NUM_CELLS;
    float noise_x = rand(float2(cell));
    float noise_y = rand(float2(cell.yx));
    return cell_base + (0.5 + 1.5 * float2(noise_x, noise_y)) / NOISE_WORLEY_NOISE_NUM_CELLS;
}

// Performs worley noise by checking all adjacent cells
// and comparing the distance to their points
float worley(float2 coord) {
    ifloat2 cell = ifloat2(coord * NOISE_WORLEY_NOISE_NUM_CELLS);
    float dist = 1.0;

    // Search in the surrounding 5x5 cell block
    for (int x = 0; x < 5; x++) { 
        for (int y = 0; y < 5; y++) {
            float2 cell_point = get_cell_point(cell + ifloat2(x-2, y-2));
            dist = min(dist, distance(cell_point, coord));

        }
    }

    dist /= length(float2(1.0 / NOISE_WORLEY_NOISE_NUM_CELLS));
    return dist;
}
#endif

// range (0, 1)
float worley3D(float3 p, float frequency, float seed = 0){

    p *= frequency;
    float3 id = floor(p);
    float3 fd = fract(p);

    float minimalDist = 1000.;

    for(float x = -1.; x <=1.; x++){
        for(float y = -1.; y <=1.; y++){
            for(float z = -1.; z <=1.; z++){

                float3 coord = float3(x,y,z);
                float3 hashCoord = id+coord;

                float3 rId = hash3(asuint(mod(hashCoord, frequency.xxx) + seed));

                float3 r = (coord + rId) - fd; 

                float d = dot(r,r);

                if(d < minimalDist){
                    minimalDist = d;
                }

            }//z
        }//y
    }//x

    return minimalDist;
}

float worley3DWrap(float3 p, float wrap){

    float3 id = floor(p);
    float3 fd = fract(p);

    float minimalDist = 1.;


    for(float x = -1.; x <=1.; x++){
        for(float y = -1.; y <=1.; y++){
            for(float z = -1.; z <=1.; z++){

                float3 coord = float3(x,y,z);
                float3 wrapped = (id+coord) % float3(wrap);

                float3 rId = hash3(asuint(wrapped));

                float3 r = coord + rId - fd; 

                float d = dot(r,r);

                if(d < minimalDist){
                    minimalDist = d;
                }

            }//z
        }//y
    }//x

    return minimalDist;
}

// source: https://www.shadertoy.com/view/ttc3zr
uint2 murmurHash21(uint src) {
    const uint M = 0x5bd1e995u;
    uint2 h = uint2(1190494759u, 2147483647u);
    src *= M; src ^= src>>24u; src *= M;
    h *= M; h ^= src;
    h ^= h>>13u; h *= M; h ^= h>>15u;
    return h;
}

uint rand_xorshift(uint rng_state)
{
    // Xorshift algorithm from George Marsaglia's paper
    rng_state ^= (rng_state << 13);
    rng_state ^= (rng_state >> 17);
    rng_state ^= (rng_state << 5);
    return rng_state;
}

// High-quality PCG hash for 32-bit uints
uint hash_u32(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// 2D variant for pixel-wise seeds
uint2 hash_u32_v2(uint2 v) {
    return uint2(
        hash_u32(v.x ^ hash_u32(v.y)),
        hash_u32(v.y ^ hash_u32(v.x))
    );
}
