#pragma once
#include "Core/Graphics/Material.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/RenderPass.hpp"

#include "Nodes/CommandBufferBeginNode.hpp"
#include "Nodes/CommandBufferEndNode.hpp"
#include "Nodes/ImageNode.hpp"
#include "Nodes/RenderPassBeginNode.hpp"
#include "Nodes/RenderPassEndNode.hpp"

#include <format>
#include <functional>
#include <string>

namespace Engine::RGraph
{
// GfxDriver is a thin wrapper around Graphis API. Graphics API like Vulkan,
// which is our main target Graphics API , need explicit dynamic resource
// management. RenderGraph split the rendering logics into 'compile' and
// 'execute' so that we have a chance to configure out at want point a dynamic
// resource is needed dynamic resource: any resource that need a memory
// management within the render pipeline

template <class T>
class NodeProperty
{
public:
    using OnChangeCallback = std::function<bool(Node* node, const T& val, const T& oldVal)>;

    NodeProperty(Node* node, OnChangeCallback onChange = nullptr) : node(node), onChange(onChange) {}
    inline NodeProperty<T>& operator=(const T& val)
    {
        T old = this->val;
        if (onChange && onChange(node, val, old)) this->val = val;
        else this->val = val;

        return *this;
    }

private:
    T val;
    Node* node;
    OnChangeCallback onChange;
};

#define RGRAPH_NODE_PROPERTY(Type, Name, Callback) NodeProperty<Type> Name = NodeProperty<Type>(this, Callback);

class RenderGraph
{
public:
    bool Compile();
    bool Execute();

    template <class T, class... Args>
    T* AddNode(const Args&... args)
    {
        auto p = new T(args...);
        static_assert(std::is_base_of_v<Node, T>);
        nodes.emplace_back((Node*)(p));
        return p;
    }

private:
    std::vector<std::unique_ptr<Node>> nodes;
};
} // namespace Engine::RGraph
