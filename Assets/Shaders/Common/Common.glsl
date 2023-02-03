
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
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 viewPos;
    float lightCount;
    Light lights[MAX_LIGHT_COUNT];
} scene;

layout(set = SET_GLOBAL, binding = 1) uniform sampler2D vtCache;
layout(set = SET_GLOBAL, binding = 2) uniform sampler2D vtIndir;

#endif
