#include "RenderPassEndNode.hpp"

namespace Engine::RGraph
{

RenderPassEndNode::RenderPassEndNode()
{
    cmdBufIn = AddPort("CmdBufIn", Port::Type::Input, typeid(CommandBuffer));
    cmdBufOut = AddPort("CmdBufOut", Port::Type::Output, typeid(CommandBuffer));
}
bool RenderPassEndNode::Compile(ResourceStateTrack& stateTrack)
{
    cmdBuf = (CommandBuffer*)GetResourceFromConnected(cmdBufIn);
    if (cmdBuf == nullptr) return false;
    cmdBufOut->GetResource().SetVal(cmdBuf);

    return true;
}
bool RenderPassEndNode::Execute(ResourceStateTrack& stateTrack)
{
    cmdBuf->EndRenderPass();
    return true;
}
} // namespace Engine::RGraph
