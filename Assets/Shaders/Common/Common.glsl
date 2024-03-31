// this file should be used for scene objects

#ifndef COMMON_INCLUDED
#define COMMON_INCLUDED

#define SET_GLOBAL 0
#define SET_PASS 1
#define SET_MATERIAL 2
#define SET_OBJECT 3

layout( push_constant ) uniform Transform
{
    mat4 model;
} pconst;

#define SCENE_INFO_BINDING 0
#include "Common/SceneInfo.glsl"

#define shadowMapSampler shadowMapSampler_sampler_linear
#define shadowMap shadowMap_sampler_linear

layout(set = SET_GLOBAL, binding = 1) uniform texture2D shadowMap;
#ifdef G_VSM
layout(set = SET_GLOBAL, binding = 2) uniform sampler shadowMapSampler;
#else
layout(set = SET_GLOBAL, binding = 3) uniform samplerShadow shadowMapSampler;
#endif
layout(set = SET_GLOBAL, binding = 4) uniform samplerCube diffuseCube;
layout(set = SET_GLOBAL, binding = 5) uniform samplerCube specularCube;

#if G_PCF
float PcfShadow(vec2 shadowCoord, float objShadowDepth)
{
    float shadow = 0;

    float x,y;

    const float steps = 2;
    const float filterSize = 1;
    const float halfFilterSize = filterSize / 2;
    const float filterStep = filterSize / steps;
    const float totalSample = (steps + 1) * (steps + 1);
    for (y = -halfFilterSize; y <= halfFilterSize; y += filterStep)
        for (x = -halfFilterSize; x <= halfFilterSize; x += filterStep)
        {
            vec2 uv = shadowCoord + vec2(x, y) * scene.shadowMapSize.zw;
            vec3 uvd = vec3(uv, objShadowDepth);
            shadow += texture(sampler2DShadow(shadowMap, shadowMapSampler), uvd).x;
        }

    return shadow / totalSample;
}
#endif

#endif
