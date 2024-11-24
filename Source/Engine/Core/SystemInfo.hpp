#pragma once

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

    static SystemInfo& Singleton();

private:
    float screenWidth;
    float screenHeight;
};
