#include "NodeBlueprint.hpp"

namespace Rendering::FrameGraph
{
std::span<NodeBlueprint> NodeBlueprintRegisteration::GetNodeBlueprints()
{
    return GetRegisteration().blueprints;
}

NodeBlueprintRegisteration& NodeBlueprintRegisteration::GetRegisteration()
{
    static NodeBlueprintRegisteration r;

    return r;
}

} // namespace Rendering::FrameGraph
