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
    Light lights[MAX_LIGHT_COUNT];
} scene;

#define shadowMapSampler shadowMapSampler_sampler_point

layout(set = SET_GLOBAL, binding = 1) uniform sampler2D vtCache;
layout(set = SET_GLOBAL, binding = 2) uniform sampler2D vtIndir;
layout(set = SET_GLOBAL, binding = 3) uniform texture2D shadowMap;
layout(set = SET_GLOBAL, binding = 4) uniform sampler shadowMapSampler;
layout(set = SET_GLOBAL, binding = 5) uniform samplerCube environmentMap;

float PcfShadow(vec2 shadowCoord, float objShadowDepth)
{
    float shadow = 36;

    float x,y;
    for (y = -2.5 ; y <=2.5 ; y+=1.0)
        for (x = -2.5 ; x <=2.5 ; x+=1.0)
        {
            float d = texture(sampler2D(shadowMap, shadowMapSampler), shadowCoord += vec2(x, y) * (1/4096.0f)).x;
            if (objShadowDepth > d)
                shadow -= 1;
        }

    shadow /= 36.0f ;

    return shadow;
}


#endif
