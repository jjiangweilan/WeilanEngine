#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED

float remap(float value, float originMin, float originMax, float newMin, float newMax) {
    return newMin + (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

float remap(float value, vec4 from2to2) {
    return from2to2.z + (value - from2to2.x) / (from2to2.y - from2to2.x) * (from2to2.w - from2to2.z);
}

vec3 remap(vec3 value, vec4 from2to2) {
    return vec3(from2to2.z) + (value - vec3(from2to2.x)) / (vec3(from2to2.y) - vec3(from2to2.x)) * (vec3(from2to2.w) - vec3(from2to2.z));
}

// rotation
mat2 Rotate2D(float angle){
    return mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
}

#endif
