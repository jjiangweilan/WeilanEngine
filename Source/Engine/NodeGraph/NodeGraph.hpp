#pragma once
#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace Engine::NGraph
{

class Node;
enum class PortType
{
    Input,
    Output
};

class NodePort
{
public:
    enum class Type
    {
        Input,
        Output
    };
    enum class CallbackType
    {
        Connect,
        Disconnect
    };

    // our port, their port, callbackType
    using Callback = std::function<void(NodePort*, NodePort*, CallbackType)>;
    NodePort(Node* node, Type type, Callback callback) : parent(node), callback(callback), type(type) {}
    virtual ~NodePort() {}
    using PortID = const std::type_info&;

    virtual PortID GetID() = 0;
    virtual const char* GetName() = 0;
    virtual bool IsCompatible(PortID id) { return true; }
    Type GetType() { return type; }

    void Disconnect(NodePort* port)
    {
        auto iter = std::find(connected.begin(), connected.end(), port);
        if (iter != connected.end())
        {
            connected.erase(iter);
            port->Disconnect(this);
            callback(this, port, CallbackType::Disconnect);
        }
    }
    void Connect(NodePort* port)
    {
        auto iter = std::find(connected.begin(), connected.end(), port);

        if (iter == connected.end())
        {
            connected.push_back(port);
            port->Connect(this);
            callback(this, port, CallbackType::Connect);
        }
    }

    Node* GetNode() { return parent; }
    const std::vector<NodePort*> GetConnectedPorts() { return connected; }

protected:
    Node* parent;
    std::vector<NodePort*> connected;
    Callback callback;
    Type type;
};
#define NGRAPH_DECLARE_PORT(ClassName, eType)                                                                          \
    ClassName(Node* node, Callback callback) : NodePort(node, Type::eType, callback) {}

template <class T> NodePort::PortID GetPortID() { return typeid(T); }

class Node
{
public:
    virtual ~Node() {}
    uint32_t GetDepth() { return depth; }
    const std::list<std::unique_ptr<NodePort>>& GetInputPorts() { return inputPorts; }
    const std::list<std::unique_ptr<NodePort>>& GetOutputPorts() { return outputPorts; }

    template <class T, class... Args> T* AddPort(Args&&... args)
    {
        T* port = new T(
            this,
            std::bind(&Node::OnPortCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
            args...);
        switch (port->GetType())
        {
            case NodePort::Type::Output: outputPorts.emplace_back(port); break;
            case NodePort::Type::Input: inputPorts.emplace_back(port); break;
        }

        return port;
    }

    template <class T> void RemovePort(T* port)
    {
        auto f = [port](const std::unique_ptr<NodePort>& other)
        {
            if (port == other.get())
            {
                for (auto p : port->GetConnectedPorts())
                {
                    p->Disconnect(port);
                }
                return true;
            }
            return false;
        };
        switch (port->GetType())
        {
            case NodePort::Type::Output: outputPorts.remove_if(f);
            case NodePort::Type::Input: inputPorts.remove_if(f);
        }
    }

protected:
    virtual void OnPortCallback(NodePort* ourPort, NodePort* theirPort, NodePort::CallbackType type)
    {
        if (type == NodePort::CallbackType::Connect)
        {
            if (ourPort->GetType() == NodePort::Type::Input)
            {
                AddDepth(theirPort->GetNode()->GetDepth() + 1);
            }
        }
        if (type == NodePort::CallbackType::Disconnect)
        {
            if (ourPort->GetType() == NodePort::Type::Input)
            {
                AddDepth(-theirPort->GetNode()->GetDepth() - 1);
            }
        }
    }

    void AddDepth(uint32_t delta)
    {
        depth += delta;
        for (auto& o : outputPorts)
        {
            for (auto p : o->GetConnectedPorts())
            {
                p->GetNode()->AddDepth(delta);
            }
        }
    }

private:
    uint32_t depth = 0;
    std::list<std::unique_ptr<NodePort>> inputPorts;
    std::list<std::unique_ptr<NodePort>> outputPorts;
};

class NodeGraph
{
public:
    NodeGraph(){};
    template <class T, class... Args> T* AddNode(const Args&... args)
    {
        auto p = std::make_unique<T>(args...);
        auto temp = p.get();
        nodes.push_back(std::move(p));
        return temp;
    }

    const std::list<std::unique_ptr<Node>>& Build()
    {
        nodes.sort([](const auto& left, const auto& right) { return left->GetDepth() < right->GetDepth(); });
        return nodes;
    }

private:
    std::list<std::unique_ptr<Node>> nodes;
};
} // namespace Engine::NGraph
