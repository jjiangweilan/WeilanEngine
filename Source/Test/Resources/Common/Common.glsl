#ifndef COMMON_INCLUDE
#define COMMON_INCLUDE
#include "Lighting/Lighting.glsl"

#define SET_GLOBAL 0   
#define SET_SHADER 1
#define SET_MATERIAL 2
#define SET_OBJECT 3

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
} scene;

// layout(std140, set = SET_GLOBAL, binding = 1) readonly buffer SceneLights {
//     Light lights[];
// } lights;

dawd
#endif
