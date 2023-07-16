#ifndef LIGHTING_INCLUDE
#define LIGHTING_INCLUDE

struct Light
{
    vec4 position;
    float range;
    float intensity;
    vec2 padding; // if not when running from xcode, it will report error
};

#endif
