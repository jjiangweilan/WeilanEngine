#ifndef SCENE_INFO_INCLUDED
#define SCENE_INFO_INCLUDED

#include "Lighting/Lighting.glsl"
#define MAX_LIGHT_COUNT 128

#ifndef SCENE_INFO_BINDING
layout(set = 0) uniform SceneInfo
#else
layout(set = 0, binding = SCENE_INFO_BINDING) uniform SceneInfo
#endif
{
    vec4 viewPos;
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 worldToShadow;
    mat4 invProjection;
    mat4 invNDCToWorld;
    vec4 lightCount; // x: lightCount
    vec4 shadowMapSize;
    vec4 cameraZBufferParams;
    vec4 cameraFrustum;// left right bottom top
    vec4 screenSize;
    vec4 cachedMainLightDirection;
    Light lights[MAX_LIGHT_COUNT];
} scene;
#endif
