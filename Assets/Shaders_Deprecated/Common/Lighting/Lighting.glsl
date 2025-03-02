#ifndef LIGHTING_INCLUDE
#define LIGHTING_INCLUDE

struct Light
{
    vec4 lightColor;
    vec4 position;
    float ambientScale;
    float range;
    float intensity;
    float pointLightTerm1;
    float pointLightTerm2;
};

#endif
