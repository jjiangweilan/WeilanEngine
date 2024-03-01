#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED

float remap(float value, float originMin, float originMax, float newMin, float newMax) {
  return newMin + (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

float remap(float value, vec4 from2to2) {
  return from2to2.z + (value - from2to2.x) / (from2to2.y - from2to2.x) * (from2to2.w - from2to2.z);
}

#endif
