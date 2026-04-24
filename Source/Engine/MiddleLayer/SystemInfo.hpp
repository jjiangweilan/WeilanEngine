#pragma once
#include "Engine/Library/Math.hpp"

class SystemInfo
{
public:
    float2 GetScreenSize() { return {screenWidth, screenHeight}; }
    void GetScreenSize(float& width, float& height);
    int2 GetSystemWindowSize();

    // when in release build game view is at 0,0
    // but when in editor game view origin may not be at 0,0
    int2 GetGameViewOrigin();

    void SetGameViewOrigin(int2 origin);
    void SetScreenSize(float width, float height);

    static SystemInfo& Singleton();

private:
    float screenWidth = 0;
    float screenHeight = 0;
    int2 gameViewOrigin = int2(0, 0);
};
