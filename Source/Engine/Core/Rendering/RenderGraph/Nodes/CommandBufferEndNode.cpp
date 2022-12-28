#include "CommandBufferEndNode.hpp"

namespace Engine::RGraph
{
CommandBufferEndNode::CommandBufferEndNode()
{
    commandBufferInPort = AddPort("In", Port::Type::Input, typeid(CommandBuffer));
}

bool CommandBufferEndNode::Preprocess(ResourceStateTrack& stateTrack) { return true; }
bool CommandBufferEndNode::Compile(ResourceStateTrack& stateTrack)
{
    auto inPortConneted = commandBufferInPort->GetConnectedPort();
    if (inPortConneted)
    {
        auto res = inPortConneted->GetResource();
        if (!res.IsNull())
        {
            cmdBuf = (CommandBuffer*)res.GetVal();
        }
    }

    return true;
}
bool CommandBufferEndNode::Execute(ResourceStateTrack& stateTrack)
{
    cmdBuf->End();
    return true;
}
} // namespace Engine::RGraph
