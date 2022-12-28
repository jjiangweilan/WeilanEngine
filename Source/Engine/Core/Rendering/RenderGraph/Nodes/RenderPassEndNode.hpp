#pragma once
#include "../Node.hpp"
#include "GfxDriver/GfxDriver.hpp"
namespace Engine::RGraph
{
class RenderPassEndNode : public Node
{
public:
    RenderPassEndNode();
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

    Port* GetPortCmdBufIn() { return cmdBufIn; }
    Port* GetPortCmdBufOut() { return cmdBufOut; }

private:
    Port* cmdBufIn;
    Port* cmdBufOut;

    /* compile data */
    Gfx::RenderPass* renderPass;
    CommandBuffer* cmdBuf;
};
} // namespace Engine::RGraph
