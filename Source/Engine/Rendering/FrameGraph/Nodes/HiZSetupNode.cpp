#pragma once
#include "../NodeBlueprint.hpp"

namespace FrameGraph
{
class HiZSetupNode : public Node
{
    DECLARE_OBJECT();

public:
    HiZSetupNode()
    {
        DefineNode();
    };
    HiZSetupNode(FGID id) : Node("HiZSetupNode", id)
    {
        DefineNode();
    }

    void Compile() override {}

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override {}

private:
    struct
    {
    } input;

    struct
    {
    } output;

    void DefineNode() {}
    static char _reg;
}; // namespace FrameGraph

char HiZSetupNode::_reg = NodeBlueprintRegisteration::Register<HiZSetupNode>("HiZ Setup");

DEFINE_OBJECT(HiZSetupNode, "C4147D77-145B-405F-9B02-CADF10CB4F86");
} // namespace FrameGraph
