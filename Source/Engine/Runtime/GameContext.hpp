#pragma once
#include "Engine/Library/Math.hpp"

class GameContext
{
public:
    float2 GetScreenSize() const { return screenSize; }

private:
    float2 screenSize;
};
