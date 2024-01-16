#pragma once
#include "../VKCommandBuffer2.hpp"

namespace Gfx
{
class VKRenderGraph
{
public:
    void Schedule(VKCommandBuffer2& cmd);

    void Execute();

private:
    class ResourceStateTrack
    {};
};
} // namespace Gfx
