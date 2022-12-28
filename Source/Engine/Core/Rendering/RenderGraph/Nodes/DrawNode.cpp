#include "DrawNode.hpp"

namespace Engine::RGraph
{
DrawNode::DrawNode()
{
    renderPassIn = AddPort("Render Pass In", Port::Type::Input, typeid(Gfx::RenderPass));
    renderPassOut = AddPort("Render Pass Out", Port::Type::Output, typeid(Gfx::RenderPass));
}

bool DrawNode::Compile(ResourceStateTrack& stateTrack)
{
    renderPassOut->GetResource().SetVal((Gfx::RenderPass*)renderPassIn->GetResource().GetVal());

    return true;
}
} // namespace Engine::RGraph
