#pragma once
#include "Engine/Library/Math.hpp"

class [[LuaClass]] SystemInfo
{
public:
    [[LuaFn]] static float2 GetScreenResolution();
    [[LuaFn]] static float2 GetScreenSize();

    int2 GetSystemWindowSize();

    // when in release build game view is at 0,0
    // but when in editor game view origin may not be at 0,0
    int2 GetGameViewOrigin();

    void SetGameViewOrigin(int2 origin);
    void SetScreenResolution(float width, float height);
    void SetScreenSize(float width, float height);

    static SystemInfo& Singleton();

private:
    float screenResolutionWidth = 0;
    float screenResolutionHeight = 0;
    float screenWidth = 0;
    float screenHeight = 0;
    int2 gameViewOrigin = int2(0, 0);
};
