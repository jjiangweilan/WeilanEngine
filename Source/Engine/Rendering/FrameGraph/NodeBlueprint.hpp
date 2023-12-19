#pragma once
#include "Nodes/Node.hpp"
#include "Rendering/RenderGraph/Graph.hpp"
#include <functional>
#include <memory>
#include <string>

namespace FrameGraph
{

class NodeBlueprint
{
public:
    NodeBlueprint(const char* name, const std::function<std::unique_ptr<Node>(FGID id)>& creation)
        : name(name), creation(creation){};

    std::unique_ptr<Node> CreateNode(FGID id) const
    {
        return creation(id);
    };

    const std::string& GetName() const
    {
        return name;
    }

private:
    std::string name;
    std::function<std::unique_ptr<Node>(FGID)> creation;
};

class NodeBlueprintRegisteration
{
public:
    static std::span<NodeBlueprint> GetNodeBlueprints();
    template <class T>
        requires std::derived_from<T, Node>
    static char Register(const char* nameID);

private:
    static NodeBlueprintRegisteration& GetRegisteration();
    std::vector<NodeBlueprint> blueprints;
};

template <class T>
    requires std::derived_from<T, Node>
char NodeBlueprintRegisteration::Register(const char* nameID)
{
    GetRegisteration().blueprints.emplace_back(nameID, [](FGID id) { return std::unique_ptr<Node>(new T(id)); });
    return '1';
}
} // namespace FrameGraph
