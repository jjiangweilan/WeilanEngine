#include "FrameContext.hpp"

FrameContext::FrameContext() : tempAllocator(64 * 1024 * 1024) {}
FrameContext& FrameContext::GetInstance()
{
    static FrameContext instance;
    return instance;
}

FrameContext& GetFrameContext()
{
    return FrameContext::GetInstance();
}

void FrameContext::BeginFrame() {}
void FrameContext::EndFrame()
{
    tempAllocator.Reset();
}
