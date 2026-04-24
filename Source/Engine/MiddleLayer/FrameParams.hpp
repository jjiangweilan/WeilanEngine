#pragma once

struct FrameParams
{
    float deltaTime = 0.0f; // Time since last frame in seconds
    float totalTime = 0.0f; // Total time since the start of the application in seconds
    int frameIndex = 0;     // Current frame index, useful for animations or effects that depend on frame count
    int viewWidth = 0;      // Width of the view or render target
    int viewHeight = 0;     // Height of the view or render target
};
