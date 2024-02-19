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

// worley noise
// from https://www.shadertoy.com/view/MstGRl
#if defined(USE_WORLEY_NOISE) && defined(NOISE_WORLEY_NOISE_NUM_CELLS)

// Arbitrary random, can be replaced with a function of your choice
float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

// Returns the point in a given cell
vec2 get_cell_point(ivec2 cell) {
	vec2 cell_base = vec2(cell) / NOISE_WORLEY_NOISE_NUM_CELLS;
	float noise_x = rand(vec2(cell));
    float noise_y = rand(vec2(cell.yx));
    return cell_base + (0.5 + 1.5 * vec2(noise_x, noise_y)) / NOISE_WORLEY_NOISE_NUM_CELLS;
}

// Performs worley noise by checking all adjacent cells
// and comparing the distance to their points
float worley(vec2 coord) {
    ivec2 cell = ivec2(coord * NOISE_WORLEY_NOISE_NUM_CELLS);
    float dist = 1.0;
    
    // Search in the surrounding 5x5 cell block
    for (int x = 0; x < 5; x++) { 
        for (int y = 0; y < 5; y++) {
        	vec2 cell_point = get_cell_point(cell + ivec2(x-2, y-2));
            dist = min(dist, distance(cell_point, coord));

        }
    }
    
    dist /= length(vec2(1.0 / NOISE_WORLEY_NOISE_NUM_CELLS));
    dist = 1.0 - dist;
    return dist;
}
#endif

#endif
