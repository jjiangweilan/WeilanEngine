#ifndef DEFERRED_SHADING_INCLUDED
#define DEFERRED_SHADING_INCLUDED
vec3 EncodeGBufferNormal(vec3 normal)
{
    return normal * 0.5 + 0.5;
}

vec3 DecodeGBufferNormal(vec3 normal)
{
    return normal * 2 - 1;
}
#endif
