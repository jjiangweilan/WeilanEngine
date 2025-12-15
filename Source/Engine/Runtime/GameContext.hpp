#pragma once
#include "Library/Math.hpp"

class GameContext
{
public:
    float2 GetScreenSize() const { return screenSize; }

private:
    float2 screenSize;
};
