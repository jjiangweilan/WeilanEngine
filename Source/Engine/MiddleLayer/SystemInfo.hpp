#pragma once
#include "Engine/Library/Math.hpp"

class SystemInfo
{
public:
    void GetScreenSize(float& width, float& height)
    {
        width = screenWidth;
        height = screenHeight;
    };
    void SetScreenSize(float width, float height)
    {
        this->screenWidth = width;
        this->screenHeight = height;
    }

    int2 GetDisplaySize();

    float2 GetScreenSize() { return {screenWidth, screenHeight}; }

    static SystemInfo& Singleton();

private:
    float screenWidth;
    float screenHeight;
};
