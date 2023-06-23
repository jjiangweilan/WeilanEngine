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
class Port
{
public:
    Port(RenderNode* parent, RenderPass::ResourceHandle handle) : parent(parent), handle(handle) {}

    Port* GetConnected() { return connected; }
    RenderNode* GetParent() { return parent; }

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
    RenderNode(std::unique_ptr<RenderPass>&& pass);

    RenderPass* GetPass() { return pass.get(); }
    std::span<Port> GetInputPorts() { return inputPorts; }
    std::span<Port> GetOutputPorts() { return outputPorts; }

    static bool Connect(Port& src, Port& dst);

private:
    std::unique_ptr<RenderPass> pass;
    std::vector<Port> inputPorts;
    std::vector<Port> outputPorts;

    // used by Graph
    int depth = -1;
    int sortIndex = -1;

    friend class Graph;
};

class Graph
{
public:
    template <class T>
    RenderNode* AddNode()
    {
        std::unique_ptr<RenderPass> pass = std::unique_ptr<RenderPass>(new T());
        nodes.emplace_back(new RenderNode(std::move(pass)));
        return nodes.back().get();
    }

    RenderNode* AddNode(const RenderPass::ExecutionFunc& execute,
                        const std::vector<RenderPass::ResourceDescription>& resourceDescs,
                        const std::vector<RenderPass::ResourceCreationRequest>& creationRequests,
                        const std::vector<RenderPass::ResourceHandle>& inputs,
                        const std::vector<RenderPass::ResourceHandle>& outputs,
                        const std::vector<RenderPass::Subpass>& subpasses)
    {
        std::unique_ptr<RenderPass> pass =
            std::make_unique<RenderPass>(execute, resourceDescs, creationRequests, inputs, outputs, subpasses);
        nodes.emplace_back(new RenderNode(std::move(pass)));
        return nodes.back().get();
    }

    RenderNode* AddNode(const RenderPass::ExecutionFunc& execute,
                        const std::vector<RenderPass::ResourceDescription>& resourceDescs,
                        const std::vector<RenderPass::ResourceCreationRequest>& creationRequests,
                        const std::vector<RenderPass::ResourceHandle>& inputs,
                        const std::vector<RenderPass::ResourceHandle>& outputs,
                        const RenderPass::Subpass& subpass)
    {
        std::unique_ptr<RenderPass> pass = std::make_unique<RenderPass>(execute,
                                                                        resourceDescs,
                                                                        creationRequests,
                                                                        inputs,
                                                                        outputs,
                                                                        std::vector<RenderPass::Subpass>{subpass});
        nodes.emplace_back(new RenderNode(std::move(pass)));
        return nodes.back().get();
    }

    // After all nodes are configured, call process once before calling Execute
    void Process();

    // execute all nodes for once
    void Execute();

private:
    class ResourcePool
    {
    public:
        Gfx::Buffer* CreateBuffer() { return nullptr; }
        Gfx::Image* CreateImage() { return nullptr; }

        void ReleaseBuffer(Gfx::Image* handle);
        void ReleaseImage(Gfx::Buffer* handle);

    private:
        std::vector<std::unique_ptr<Gfx::Image>> images;
        std::vector<std::unique_ptr<Gfx::Buffer>> buffers;
    } resourcePool;

    class ResourceOwner
    {
    public:
        ResourceOwner(Gfx::Image* image) : resourceRef(image, this) {}
        ResourceOwner(Gfx::Buffer* buffer) : resourceRef(buffer, this) {}

        ResourceRef resourceRef;
        std::vector<bool> used;
    };

    std::vector<std::unique_ptr<RenderNode>> nodes;

    std::vector<std::unique_ptr<ResourceOwner>> resourceOwners;

    // preprocess the nodes, expect the graph already sorted.
    // It will create resource required. Set resources from input ports
    void Preprocess(RenderNode* node);
    void Compile();
    ResourceOwner* CreateResourceOwner(const RenderPass::ResourceCreationRequest& request, int originalNode);

    static int GetDepthOfNode(RenderNode* node);
};
} // namespace Engine::RenderGraph
