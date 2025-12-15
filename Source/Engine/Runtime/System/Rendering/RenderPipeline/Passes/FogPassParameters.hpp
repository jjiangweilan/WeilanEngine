#pragma once
#include "Library/Math.hpp"

struct FogPassParameters
{
    bool enabled = true;
    float4 fogColor = float4(0.3, 0.3, 0.3, 0.3);
    float fogDensity = 1.0f;
};
