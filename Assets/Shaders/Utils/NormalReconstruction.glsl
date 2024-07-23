#ifndef NORMAL_RECONSTRUCTION_INCLUDED
#define NORMAL_RECONSTRUCTION_INCLUDED

#include "SpaceConversion.glsl"

vec3 NormalReconstructionLow(vec3 viewPos)
{
    //dFdx returns either dFdxCoarse or dFdxFine. dFdy returns either dFdyCoarse or dFdyFine.
    return normalize(cross(dFdx(viewPos), dFdy(viewPos)));
}

// 5 tap reconstruction
// expect clampped depth
// invNDCToX takes invNDCProject or invNDCToWorld
vec3 NormalReconstructionMedium(sampler2D depthTex, vec2 uv, vec2 depthTexelSize, vec4 cameraZBufferParams, mat4 invNDCToX, out vec3 centerPos)
{
    float depths[5] = {
        texture(depthTex, uv).x,
        texture(depthTex, uv - vec2(depthTexelSize.x, 0)).x,
        texture(depthTex, uv + vec2(depthTexelSize.x, 0)).x,
        texture(depthTex, uv - vec2(0, depthTexelSize.y)).x,
        texture(depthTex, uv + vec2(0, depthTexelSize.y)).x
    };

    const float depthC = LinearEyeDepth(depths[0], cameraZBufferParams);
    const float depthL = LinearEyeDepth(depths[1], cameraZBufferParams);
    const float depthR = LinearEyeDepth(depths[2], cameraZBufferParams);
    int h = abs(depthC - depthL) < abs(depthC - depthR) ? 1 : 2;

    const float depthU = LinearEyeDepth(depths[3], cameraZBufferParams);
    const float depthB = LinearEyeDepth(depths[4], cameraZBufferParams);
    int v = abs(depthC - depthU) < abs(depthC - depthB) ? 3 : 4;

    vec4 posC = invNDCToX * vec4(uv * 2 - 1, depths[0], 1.0f);
    posC.xyz /= posC.w;

    vec4 posH, posV;
    posH = invNDCToX * vec4((uv + vec2((2 * (h - 1) - 1) * depthTexelSize.x, 0)) * 2 - 1, (h == 1 ? depths[1] : depths[2]), 1.0f);
    posH.xyz /= posH.w;
    posV = invNDCToX * vec4((uv + vec2(0, (2 * (v - 3) - 1) * depthTexelSize.y)) * 2 - 1, (v == 3 ? depths[3] : depths[4]), 1.0f);
    posV.xyz /= posV.w;

    vec3 n0, n1;
    if ((h == 1 && v == 3) || (h == 2 && v == 4))
    {
        n0 = posV.xyz;
        n1 = posH.xyz;
    }
    else
    {
        n0 = posH.xyz;
        n1 = posV.xyz;
    }

    vec3 normal = normalize(cross(n1 - posC.xyz, n0 - posC.xyz));

    centerPos = posC.xyz;
    return normal;
}

#endif
