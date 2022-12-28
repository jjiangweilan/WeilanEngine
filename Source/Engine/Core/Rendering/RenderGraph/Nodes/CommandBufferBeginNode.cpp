#include "CommandBufferBeginNode.hpp"
#include "../CommandPoolManager.hpp"

namespace Engine::RGraph
{
CommandBufferBeginNode::CommandBufferBeginNode()
{
    cmdBufOutput = AddPort("Out", Port::Type::Output, typeid(CommandBuffer));
}

bool CommandBufferBeginNode::Compile(ResourceStateTrack& stateTrack)
{
    cmdBuf = GetCommandPoolManager()->GetCommandBuffer(0);
    cmdBufOutput->GetResource().SetVal(cmdBuf.Get());

    return true;
}
bool CommandBufferBeginNode::Execute(ResourceStateTrack& stateTrack)
{
    cmdBuf->Begin();
    return true;
}
} // namespace Engine::RGraph
