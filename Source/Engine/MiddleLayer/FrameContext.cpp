#include "FrameContext.hpp"

FrameContext::FrameContext() {}
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
void FrameContext::EndFrame() {}
