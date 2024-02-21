#ifndef UTIL_INCLUDED
#define UTIL_INCLUDED

float remap(float value, float originMin, float originMax, float newMin, float newMax) {
  return newMin + (value - originMin) / (originMax - originMin) * (newMax - newMin);
}

#endif
