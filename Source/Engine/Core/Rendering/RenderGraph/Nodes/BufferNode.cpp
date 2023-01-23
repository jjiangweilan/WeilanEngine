#include "BufferNode.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <spdlog/spdlog.h>

namespace Engine::RGraph
{
bool BufferNode::Compile(ResourceStateTrack& stateTrack)
{
    auto& s = stateTrack.GetState(&bufferRes);

    if (props.size == -1)
    {
        SPDLOG_WARN("BufferNode-propsSize: equal zero");
    }
    if (s.bufferUsages == Gfx::BufferUsage::None)
    {
        SPDLOG_WARN("BufferNode-bufferUsages: equal zero");
    }

    Gfx::Buffer::CreateInfo createInfo;
    createInfo.size = props.size;
    createInfo.usages = s.bufferUsages;
    createInfo.debugName = props.debugName;
    createInfo.visibleInCPU = props.visibleInCPU;

    buffer = GetGfxDriver()->CreateBuffer(createInfo);
    bufferRes.SetVal(buffer.Get());

    return true;
}
} // namespace Engine::RGraph
