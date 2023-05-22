#pragma once
#include "GraphContext.hpp"
#include <span>
#include <string_view>
#include <vector>

namespace Engine::RenderGraph
{
class Node;
class Port
{
public:
    Port(std::string_view portName,
         Node* parent,
         ResourceType type,
         bool isOutput,
         const std::function<bool(Port*)>& onResourceChange = nullptr) // called for input port
        : name(portName), isOutput(isOutput), parent(parent), connected(nullptr), onResourceChange(onResourceChange),
          resourceRef(nullptr)
    {}

    Node* GetParent() { return parent; }
    Port* GetConnected() { return connected; }
    const std::string& GetName() { return name; }

protected:
    // a resource type that this port should connect to
    bool isOutput;
    ResourceType type = ResourceType::Null;
    std::function<bool(Port* self)> onResourceChange = nullptr;
    std::string name;
    Node* parent = nullptr;
    Port* connected = nullptr;
    ResourceRef resourceRef = nullptr;

    friend class Graph;
    friend class Node;
};

class Node
{
public:
    Node(GraphContext& context) : graphContext(&context) {}
    virtual bool Preprocess() { return true; }
    virtual bool Compile() { return true; }
    virtual bool Execute() { return true; }

    Port* GetInputPort(std::string_view name)
    {
        for (auto& p : inputPorts)
        {
            if (p->GetName() == name)
                return p;
        }
        return nullptr;
    }

    Port* GetOutputPort(std::string_view name)
    {
        for (auto& p : outputPorts)
        {
            if (p->GetName() == name)
                return p;
        }
        return nullptr;
    }
    std::span<Port*> GetOutputPorts() { return outputPorts; }
    std::span<Port*> GetInputPorts() { return inputPorts; }

protected:
    std::vector<Port*> inputPorts;
    std::vector<Port*> outputPorts;
    GraphContext* graphContext;

    template <class... Args>
    std::unique_ptr<Port> CreatePort(Args&&... args)
    {
        std::unique_ptr<Port> port = std::make_unique<Port>(std::forward<Args>(args)...);
        if (port->isOutput)
            outputPorts.push_back(port.get());
        else
            inputPorts.push_back(port.get());

        return port;
    }

    friend class Graph;
};
template <class T>
concept IsNode = std::derived_from<T, Node>;
} // namespace Engine::RenderGraph
