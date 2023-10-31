#include "NodeBlueprint.hpp"

namespace Engine::FrameGraph
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

} // namespace Engine::FrameGraph
