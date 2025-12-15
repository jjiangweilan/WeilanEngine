#pragma once
#include "Library/DynamicArray.hpp"
#include <array>

class FrameContext
{
public:
    static FrameContext& GetInstance();

    void BeginFrame();
    void EndFrame();

private:
    FrameContext();

    bool isAlive; // is this frame currently being processed?
};

FrameContext& GetFrameContext();
