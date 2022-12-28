#pragma once
#include "../Node.hpp"

namespace Engine::RGraph
{
class CommandBufferEndNode : public Node
{
public:
    CommandBufferEndNode();
    bool Preprocess(ResourceStateTrack& stateTrack) override;
    bool Compile(ResourceStateTrack& stateTrack) override;
    bool Execute(ResourceStateTrack& stateTrack) override;

private:
    Port* commandBufferInPort;

    /* compiled data */
    CommandBuffer* cmdBuf;
};
} // namespace Engine::RGraph
