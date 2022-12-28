#pragma once
#include "../Node.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine::RGraph
{
class CommandBufferBeginNode : public Node
{
public:
    CommandBufferBeginNode();

    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

    Port* GetPortOutput() { return cmdBufOutput; }

private:
    Port* cmdBufOutput;
    UniPtr<CommandBuffer> cmdBuf;
    Gfx::CommandPool* commandPool;
};
} // namespace Engine::RGraph
