// this file should be used for scene objects

#ifndef COMMON_INCLUDE
#define COMMON_INCLUDE
#include "Lighting/Lighting.glsl"

#define SET_GLOBAL 0
#define SET_SHADER 1
#define SET_MATERIAL 2
#define SET_OBJECT 3
#define MAX_LIGHT_COUNT 32

layout( push_constant ) uniform Transform
{
    mat4 model;
} pconst;

layout(set = SET_GLOBAL, binding = 0) uniform SceneInfo
{
    vec4 viewPos;
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 worldToShadow;
    vec4 lightCount; // x: lightCount
    vec4 shadowMapSize;
    Light lights[MAX_LIGHT_COUNT];
} scene;

#define shadowMapSampler shadowMapSampler_sampler_linear
#define shadowMap shadowMap_sampler_linear

layout(set = SET_GLOBAL, binding = 1) uniform sampler2D vtCache;
layout(set = SET_GLOBAL, binding = 2) uniform sampler2D vtIndir;
layout(set = SET_GLOBAL, binding = 3) uniform sampler2DShadow shadowMap;
layout(set = SET_GLOBAL, binding = 4) uniform sampler shadowMapSampler;
layout(set = SET_GLOBAL, binding = 5) uniform samplerCube environmentMap;

float PcfShadow(vec2 shadowCoord, float objShadowDepth)
{
    float shadow = 0;

    float x,y;

    const float steps = 2;
    const float filterSize = 3;
    const float halfFilterSize = filterSize / 2;
    const float filterStep = filterSize / steps;
    const float totalSample = (steps + 1) * (steps + 1);
    for (y = -halfFilterSize; y <= halfFilterSize; y += filterStep)
        for (x = -halfFilterSize; x <= halfFilterSize; x += filterStep)
        {
            vec2 uv = shadowCoord + vec2(x, y) * scene.shadowMapSize.zw;
            vec3 uvd = vec3(uv, objShadowDepth);
            float d = texture(shadowMap, uvd).x;
            shadow += d;
        }

    return shadow / totalSample;
}


#endif
