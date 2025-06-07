#pragma once
#include "Libs/Allocator/LinearAllocator.hpp"
#include <array>
#include <vector>

class FrameContext
{
public:
    using TempAllocator = LinearAllocator<uint8_t>;

    static FrameContext& GetInstance();
    TempAllocator& GetTempAllocator() { return tempAllocator; }

    void BeginFrame();
    void EndFrame();

private:
    FrameContext();
    TempAllocator tempAllocator;

    bool isAlive; // is this frame currently being processed?
};

FrameContext& GetFrameContext();
