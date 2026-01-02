#pragma once
#include "Engine/Library/Math.hpp"

struct [[SerClass]] FogPassParameters
{
    bool enabled = true;
    float4 fogColor = float4(0.3, 0.3, 0.3, 0.3);
    float fogDensity = 1.0f;
};
