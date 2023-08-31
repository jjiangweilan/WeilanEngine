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

// A RenerNode contains a RenderPass and act like a window for the RenderPass to talk to other nodes.
class RenderNode
{
public:
    RenderNode(std::unique_ptr<RenderPass>&& pass, const std::string& debugDesc = "");

    RenderPass* GetPass()
    {
        return pass.get();
    }

private:
    class Port
    {
    public:
        ResourceHandle handle;
        Port* connected;
        RenderNode* parent;

        friend class Graph;
        friend class RenderNode;
    };

    std::string debugDesc;
    std::unique_ptr<RenderPass> pass;
    std::vector<std::unique_ptr<Port>> inputPorts;
    std::vector<std::unique_ptr<Port>> outputPorts;

    // used by Graph
    int depth = -1;
    SortIndex sortIndex = -1;

    friend class Graph;
};

struct Attachment
{
    std::string name;
    ResourceHandle handle;
    Gfx::ImageFormat format = Gfx::ImageFormat::R8G8B8A8_UNorm;

    Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
    uint32_t mipLevels = 1;
    bool isCubemap = false;
    Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
    Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::Clear;
    Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::Store;

    Gfx::Image* externalImage;
};

struct Subpass
{
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<Attachment> colors;
    std::optional<Attachment> depth;
};

class Graph
{
public:
    virtual ~Graph();
    // template <class T>
    // RenderNode* AddNode()
    // {
    //     std::unique_ptr<RenderPass> pass = std::unique_ptr<RenderPass>(new T());
    //     nodes.emplace_back(new RenderNode(std::move(pass)));
    //     return nodes.back().get();
    // }
    //
    //
    RenderNode* AddNode(const RenderPass::ExecutionFunc& execute, const std::vector<Subpass>& subpasses)
    {
        std::vector<RenderPass::ResourceDescription> resourceDescriptions;
        for (const Subpass& subpass : subpasses)
        {
            for (auto& color : subpass.colors)
            {
                // there must be write mask but we only add read mask when the color.loadOp is Load or Clear
                RenderPass::ResourceDescription desc{
                    .name = color.name,
                    .handle = color.handle,
                    .accessFlags = Gfx::AccessMask::Color_Attachment_Write |
                                   ((color.loadOp == Gfx::AttachmentLoadOperation::Load ||
                                     color.loadOp == Gfx::AttachmentLoadOperation::Clear)
                                        ? Gfx::AccessMask::Color_Attachment_Read
                                        : Gfx::AccessMask::None),
                    .stageFlags = Gfx::PipelineStage::Color_Attachment_Output,
                    .imageUsagesFlags = Gfx::ImageUsage::ColorAttachment,
                    .imageLayout = Gfx::ImageLayout::Color_Attachment,
                    .imageCreateInfo =
                        {
                            .width = subpass.width,
                            .height = subpass.height,
                            .format = color.format,
                            .multiSampling = color.multiSampling,
                            .mipLevels = color.mipLevels,
                            .isCubemap = color.isCubemap,
                        },
                    .externalImage = color.externalImage,
                };

                resourceDescriptions.push_back(desc);
            }

            if (subpass.depth.has_value())
            {
                RenderPass::ResourceDescription desc{
                    .name = subpass.depth->name,
                    .handle = subpass.depth->handle,
                    .accessFlags = Gfx::AccessMask::Depth_Stencil_Attachment_Write |
                                   ((subpass.depth->loadOp == Gfx::AttachmentLoadOperation::Load ||
                                     subpass.depth->loadOp == Gfx::AttachmentLoadOperation::Clear)
                                        ? Gfx::AccessMask::Depth_Stencil_Attachment_Read
                                        : Gfx::AccessMask::None),
                    .stageFlags = Gfx::PipelineStage::Early_Fragment_Tests | Gfx::PipelineStage::Late_Fragment_Tests,
                    .imageUsagesFlags = Gfx::ImageUsage::DepthStencilAttachment,
                    .imageLayout = Gfx::ImageLayout::Depth_Stencil_Attachment,
                    .imageCreateInfo =
                        {
                            .width = subpass.width,
                            .height = subpass.height,
                            .format = subpass.depth->format,
                            .multiSampling = subpass.depth->multiSampling,
                            .mipLevels = subpass.depth->mipLevels,
                            .isCubemap = subpass.depth->isCubemap,
                        },
                    .externalImage = subpass.depth->externalImage,
                };

                resourceDescriptions.push_back(desc);
            }
        }

        std::vector<RenderPass::Subpass> lSubpasses; // l = lower level

        return AddNode(execute, resourceDescriptions, lSubpasses);
    }

    RenderNode* AddNode(
        const RenderPass::ExecutionFunc& execute,
        const std::vector<RenderPass::ResourceDescription>& resourceDescs,
        const std::vector<RenderPass::Subpass>& subpasses
    )
    {
        std::unique_ptr<RenderPass> pass = std::make_unique<RenderPass>(execute, resourceDescs, subpasses);
        nodes.emplace_back(new RenderNode(std::move(pass)));
        return nodes.back().get();
    }

    static void Connect(RenderNode* src, ResourceHandle srcHandle, RenderNode* dst, ResourceHandle dstHandle);

    // After all nodes are configured, call process once before calling Execute
    // the graph handles the transition of swapchain image, set the resourceHandle of the presentNode to the output of
    // the swapchain image
    void Process(RenderNode* presentNode, ResourceHandle resourceHandle);
    virtual void Process();

    // used before Execute to override external resource state that can't be tracked by the graph
    void OverrideResourceState();

    // execute all nodes for once
    virtual void Execute(Gfx::CommandBuffer& cmd);

    void Clear();

private:
    class ResourcePool
    {
    public:
        Gfx::Buffer* CreateBuffer(const Gfx::Buffer::CreateInfo& createInfo);
        Gfx::Image* CreateImage(const Gfx::ImageDescription& imageDesc, Gfx::ImageUsageFlags usages);

        void ReleaseBuffer(Gfx::Buffer* handle);
        void ReleaseImage(Gfx::Image* handle);

        void Clear()
        {
            images.clear();
            buffers.clear();
        }

    private:
        std::vector<std::unique_ptr<Gfx::Image>> images;
        std::vector<std::unique_ptr<Gfx::Buffer>> buffers;
    };

    // keep track of where the resource is used in the node, so that we can create resource barriers for them
    class ResourceOwner
    {
    public:
        ResourceOwner(const RenderPass::ResourceDescription& request)
            : request(request), resourceRef(request.type, this){};

        ResourcePool* pool;
        ResourceRef resourceRef;

        RenderPass::ResourceDescription request;

        std::vector<std::pair<SortIndex, ResourceHandle>> used;
        void Finalize(ResourcePool& pool);
    };

    // preprocess the nodes, expect the graph already sorted.
    // It will create resource required. Set resources from input ports
    void Preprocess(RenderNode* node);
    void Compile();
    ResourceOwner* CreateResourceOwner(const RenderPass::ResourceDescription& request);

    static int GetDepthOfNode(RenderNode* node);

protected:
    // rendering related stuffs, maybe factor out of Graph?
private:
    std::vector<std::unique_ptr<RenderNode>> nodes;
    std::vector<RenderNode*> sortedNodes;
    std::vector<std::unique_ptr<RenderNode>> barrierNodes;
    std::vector<std::unique_ptr<ResourceOwner>> resourceOwners;
    ResourcePool resourcePool;
}; // namespace Engine::RenderGraph
} // namespace Engine::RenderGraph
