#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "RenderPass.hpp"
#include <list>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace Engine::RenderGraph
{
class RenderNode;
using SortIndex = int;
class Port
{
public:
    Port(RenderNode* parent, RenderPass::ResourceHandle handle) : parent(parent), handle(handle) {}

    Port* GetConnected()
    {
        return connected;
    }
    RenderNode* GetParent()
    {
        return parent;
    }

private:
    RenderPass::ResourceHandle handle;
    Port* connected = nullptr;
    RenderNode* parent;

    friend class Graph;
    friend class RenderNode;
};

// A RenerNode contains a RenderPass and act like a window for the RenderPass to talk to other nodes.
class RenderNode
{
public:
    RenderNode(std::unique_ptr<RenderPass>&& pass, const std::string& debugDesc = "");

    RenderPass* GetPass()
    {
        return pass.get();
    }
    std::span<Port> GetInputPorts()
    {
        return inputPorts;
    }
    std::span<Port> GetOutputPorts()
    {
        return outputPorts;
    }

    static bool Connect(Port& src, Port& dst);

private:
    std::string debugDesc;
    std::unique_ptr<RenderPass> pass;
    std::vector<Port> inputPorts;
    std::vector<Port> outputPorts;

    // used by Graph
    int depth = -1;
    SortIndex sortIndex = -1;

    friend class Graph;
};

class Graph
{
public:
    // template <class T>
    // RenderNode* AddNode()
    // {
    //     std::unique_ptr<RenderPass> pass = std::unique_ptr<RenderPass>(new T());
    //     nodes.emplace_back(new RenderNode(std::move(pass)));
    //     return nodes.back().get();
    // }
    //
    RenderNode* AddNode(
        const RenderPass::ExecutionFunc& execute,
        const std::vector<RenderPass::ResourceDescription>& resourceDescs,
        const std::vector<RenderPass::ResourceHandle>& creationRequests,
        const std::vector<RenderPass::ResourceHandle>& inputs,
        const std::vector<RenderPass::ResourceHandle>& outputs,
        const std::vector<RenderPass::Subpass>& subpasses
    )
    {
        std::unique_ptr<RenderPass> pass =
            std::make_unique<RenderPass>(execute, resourceDescs, creationRequests, inputs, outputs, subpasses);
        nodes.emplace_back(new RenderNode(std::move(pass)));
        return nodes.back().get();
    }

    // After all nodes are configured, call process once before calling Execute
    // the graph handles the transition of swapchain image, set presentPort to the output of the swapchain image
    void Process(Port* presentPort);

    // used before Execute to override external resource state that can't be tracked by the graph
    void OverrideResourceState();

    // execute all nodes for once
    void Execute();

private:
    class ResourcePool
    {
    public:
        Gfx::Buffer* CreateBuffer();
        Gfx::Image* CreateImage(const Gfx::ImageDescription& imageDesc, Gfx::ImageUsageFlags usages);

        void ReleaseBuffer(Gfx::Buffer* handle);
        void ReleaseImage(Gfx::Image* handle);

    private:
        std::vector<std::unique_ptr<Gfx::Image>> images;
        std::vector<std::unique_ptr<Gfx::Buffer>> buffers;
    } resourcePool;

    // keep track of where the resource is used in the node, so that we can create resource barriers for them
    class ResourceOwner
    {
    public:
        ResourceOwner(const RenderPass::ResourceDescription& request)
            : request(request), resourceRef(request.type, this){};

        ResourcePool* pool;
        ResourceRef resourceRef;

        RenderPass::ResourceDescription request;

        std::vector<std::pair<SortIndex, RenderPass::ResourceHandle>> used;
        void Finalize(ResourcePool& pool);
    };

    std::vector<std::unique_ptr<RenderNode>> nodes;
    std::vector<RenderNode*> sortedNodes;
    std::vector<std::unique_ptr<RenderNode>> barrierNodes;
    std::vector<std::unique_ptr<ResourceOwner>> resourceOwners;
    std::unique_ptr<RenderNode> presentNode;

    // preprocess the nodes, expect the graph already sorted.
    // It will create resource required. Set resources from input ports
    void Preprocess(RenderNode* node);
    void Compile();
    ResourceOwner* CreateResourceOwner(const RenderPass::ResourceDescription& request);

    static int GetDepthOfNode(RenderNode* node);

    // rendering related stuffs, maybe factor out of Graph?
private:
    RefPtr<CommandQueue> queue;
    std::unique_ptr<Gfx::Semaphore> submitSemaphore;
    std::unique_ptr<Gfx::Fence> submitFence;
    std::unique_ptr<Gfx::Semaphore> swapchainAcquireSemaphore;
    std::unique_ptr<Gfx::CommandPool> commandPool;
    std::unique_ptr<CommandBuffer> mainCmd;
};
} // namespace Engine::RenderGraph
