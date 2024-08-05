#ifndef GRID_INCLUDED
#define GRID_INCLUDED
// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8#1e7c
float PristineGrid(vec2 uv, vec2 lineWidth)
{
    lineWidth = clamp(lineWidth, 0, 1);
    vec4 uvDDXY = vec4(dFdx(uv), dFdy(uv));
    vec2 uvDeriv = vec2(length(uvDDXY.xz), length(uvDDXY.yw));
    bvec2 invertLine = greaterThan(lineWidth, vec2(0.5));
    vec2 targetWidth = mix(lineWidth, vec2(1.0) - lineWidth, invertLine);
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = max(uvDeriv, 0.000001) * 1.5;
    vec2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV = mix(1.0 - gridUV, gridUV, invertLine);
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0, 1);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0, 1));
    grid2 = mix(grid2, 1.0 - grid2, invertLine );
    return mix(grid2.x, 1.0, grid2.y);
}

int GetMaxIndex(vec3 val)
{
    if (val.x > val.y && val.x > val.z) return 0;
    if (val.y > val.z) return 1;
    return 2;
}

int GetMaxIndex(vec2 val)
{
    if (val.x > val.y) return 0;
    return 1;
}

vec2 GetFirstTwoLargestValue(vec3 val, out ivec2 oIndex)
{
    int max0 = GetMaxIndex(val);
    ivec2 index = ivec2((max0 + 1) % 3, (max0 + 2) % 3);
    vec2 order2 = vec2(val[index.x], val[index.y]);
    int max1 = GetMaxIndex(order2);
    vec2 result = vec2(val[max0], order2[max1]);
    oIndex = ivec2(max0, index[max1]);
    return result;
}

float PristineGrid3D(vec3 pos, vec2 lineWidth)
{
    lineWidth = clamp(lineWidth, 0, 1);
    vec3 posDDX = dFdx(pos);
    vec3 posDDY = dFdy(pos);

    vec3 posDeriv3 = vec3(length(vec2(posDDX.x, posDDY.x)), length(vec2(posDDX.y, posDDY.y)), length(vec2(posDDX.z, posDDY.z)));

    ivec2 index;
    vec2 uvDeriv = GetFirstTwoLargestValue(posDeriv3, index);
    bvec2 invertLine = greaterThan(lineWidth, vec2(0.5));
    vec2 targetWidth = mix(lineWidth, vec2(1.0) - lineWidth, invertLine);
    vec2 drawWidth = clamp(targetWidth, uvDeriv, vec2(0.5));
    vec2 lineAA = max(uvDeriv, 0.000001) * 1.5;
    // vec2 gridUV = abs(fract(vec2(pos[index.x], pos[index.y])) * 2.0 - 1.0);
    vec2 gridUV = abs(fract(vec2(pos[index.x], pos[index.y])) * 2.0 - 1.0);
    gridUV = mix(1.0 - gridUV, gridUV, invertLine);
    vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0, 1);
    grid2 = mix(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0, 1));
    grid2 = mix(grid2, 1.0 - grid2, invertLine );
    return mix(grid2.x, 1.0, grid2.y);
}

#endif
