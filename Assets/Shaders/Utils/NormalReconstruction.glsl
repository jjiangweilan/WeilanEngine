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
vec3 NormalReconstructionMedium(sampler2D depthTex, vec2 uv, vec2 depthTexelSize, vec4 cameraZBufferParams, mat4 invNDCToX)
{
    float depths[5] = {
        texture(depthTex, uv).x,
        texture(depthTex, uv - depthTexelSize.x).x,
        texture(depthTex, uv + depthTexelSize.x).x,
        texture(depthTex, uv - depthTexelSize.y).x,
        texture(depthTex, uv + depthTexelSize.y).x
    };

    const float depthC = LinearEyeDepth(depths[0], cameraZBufferParams);
    const float depthL = LinearEyeDepth(depths[1], cameraZBufferParams);
    const float depthR = LinearEyeDepth(depths[2], cameraZBufferParams);
    int h = abs(depthC - depthL) < abs(depthC - depthR) ? 1 : 2;

    const float depthU = LinearEyeDepth(depths[3], cameraZBufferParams);
    const float depthB = LinearEyeDepth(depths[4], cameraZBufferParams);
    int v = abs(depthC - depthU) < abs(depthC - depthB) ? 3 : 4;

    vec4 posC = invNDCToX * vec4(uv * 2 - 1, depths[0], 1.0f);
    posC.xyz / posC.w;

    vec4 posX[2];
    posX[0] = invNDCToX * vec4((uv + vec2((2 * (h - 1) - 1) * depthTexelSize.x, 0)) * 2 - 1, depths[h], 1.0f);
    posX[0].xyz /= posX[0].w;
    posX[1] = invNDCToX * vec4((uv + vec2((2 * (v - 3) - 1) * depthTexelSize.y, 0)) * 2 - 1, depths[v], 1.0f);
    posX[1].xyz /= posX[1].w;

    int i,j;
    if ((h == 1 && v == 3) || (h == 2 && v == 4))
    {
        i = 1;
        j = 0;
    }
    else
    {
        i = 0;
        j = 1;
    }

    vec3 normal = normalize(cross(posX[i].xyz - posC.xyz, posX[j].xyz - posC.xyz));

    return normal * 0.5 + 0.5;
}

#endif
