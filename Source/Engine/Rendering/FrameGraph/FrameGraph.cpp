#include "FrameGraph.hpp"

namespace Engine::FrameGraph
{
void BuildGraph(Graph& graph, nlohmann::json& j)
{
    nlohmann::json nodes = j.value("nodes", {});
    if (!nodes.is_null())
    {
        for (auto& n : nodes)
        {
            std::string id = j.value("nodeID", "");
        }
    }
}
} // namespace Engine::FrameGraph
