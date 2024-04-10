#ifndef LIGHTING_INCLUDE
#define LIGHTING_INCLUDE

struct Light
{
    vec4 lightColor;
    vec4 position;
    float ambientScale;
    float range;
    float intensity;
    float padding; // if not when running from xcode, it will report error
};

#endif
