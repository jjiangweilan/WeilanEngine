#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "RenderPass.hpp"
#include "ResourceCenter.hpp"
#include <memory>

namespace Engine::RenderGraph
{

// A high level abstraction of game rendering. For example, Rendering all the opaque objects, rendering shadows,
// velocity pass, TAA pass.
class RenderPass
{
public:
    using ResourceHandle = int;

    struct ResourceDescription
    {
        std::string name;
        ResourceHandle handle;
        ResourceType type;

        union
        {
            Gfx::BufferUsageFlags bufferUsageFlags;
            Gfx::ImageUsageFlags imageUsagesFlags;
        };

        Gfx::AccessMaskFlags accessFlags;
        Gfx::PipelineStageFlags stageFlags;
        Gfx::ImageLayout imageLayout;
    };

    struct ResourceCreationRequest
    {
        std::string name;
        ResourceHandle handle;
        ResourceType type;
        union
        {
            Gfx::ImageUsageFlags imageUsagesFlags;
            Gfx::BufferUsageFlags bufferUsageFlags;
        };

        union
        {
            Gfx::ImageDescription image;
            Gfx::Buffer::CreateInfo buffer;
        };

        Gfx::AccessMaskFlags accessFlags;
        Gfx::PipelineStageFlags stageFlags;
        Gfx::ImageLayout imageLayout;
    };

    struct Attachment
    {
        ResourceHandle handle;
        Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
        Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Load;
        Gfx::AttachmentStoreOperation storeOp = Gfx::AttachmentStoreOperation::Store;
        Gfx::AttachmentLoadOperation stencilLoadOp = Gfx::AttachmentLoadOperation::DontCare;
        Gfx::AttachmentStoreOperation stencilStoreOp = Gfx::AttachmentStoreOperation::DontCare;
    };

    struct Subpass
    {
        std::vector<Attachment> colors;
        std::optional<Attachment> depth;
    };

public:
    using ResourceRefs = std::unordered_map<ResourceHandle, ResourceRef>;
    using Buffers = std::unordered_map<ResourceHandle, ResourceRef>;
    using ExecutionFunc = std::function<void(CommandBuffer&, Gfx::RenderPass& renderPass, const Buffers& buffers)>;

    RenderPass(const ExecutionFunc& execute,
               const std::vector<ResourceDescription>& resourceDescs,
               const std::vector<ResourceCreationRequest>& creationRequests,
               const std::vector<ResourceHandle>& inputs,
               const std::vector<ResourceHandle>& outputs,
               const std::vector<Subpass>& subpasses)
        : resourceDescriptions(resourceDescs), creationRequests(creationRequests), inputs(inputs), outputs(outputs),
          subpasses(subpasses), execute(execute)
    {
        resourceRefs.reserve(resourceDescs.size());
    }
    ~RenderPass() {}
    // Set resource that is needed by this node.framgraph
    void SetResourceRef(ResourceHandle handle, ResourceRef resource) { resourceRefs[handle] = resource; }

    // Get a resource from the RenderPass.
    ResourceRef GetResourceRef(ResourceHandle handle) { return resourceRefs[handle]; }

    // Get ResourceCreationRequests, a ResourceCreationRequests describe
    virtual std::span<const ResourceCreationRequest> GetResourceCreationRequests() { return creationRequests; }

    virtual const ResourceDescription& GetReferenceResourceDescription(ResourceHandle handle)
    {
        return resourceDescriptions[handle];
    }

    // Get resource inputs, use GetReferenceResourceDescription to retrive information about this resource
    std::span<const ResourceHandle> GetResourceInputs() { return inputs; }

    // Get resource outputs, use GetReferenceResourceDescription to retrive information about this resource
    std::span<const ResourceHandle> GetResourceOutputs() { return outputs; }

    // Recording all the gfx commands.
    // Don't insert any pipeline barriers for resources from outside, it's managed by the frame
    virtual void Execute(CommandBuffer& cmdBuf)
    {
        if (execute)
            execute(cmdBuf, *renderPass, buffers);
    };

    // call when all the resources are set
    void Finalize()
    {
        renderPass = GetGfxDriver()->CreateRenderPass();

        for (auto& subpass : subpasses)
        {
            std::vector<Gfx::RenderPass::Attachment> colors;
            std::optional<Gfx::RenderPass::Attachment> depth = std::nullopt;

            for (Attachment colorHandle : subpass.colors)
            {
                Gfx::Image* image = (Gfx::Image*)resourceRefs[colorHandle.handle].GetResource();
                colors.push_back({
                    .image = image,
                });
            }

            renderPass->AddSubpass(colors, depth);
        }
        for (auto& r : resourceRefs)
        {
            if (r.second.IsType(ResourceType::Buffer))
            {
                buffers[r.first] = r.second;
            }
        }
    }

protected:
    std::unique_ptr<Gfx::RenderPass> renderPass;
    std::vector<ResourceCreationRequest> creationRequests;
    const std::vector<Subpass> subpasses;
    ResourceRefs resourceRefs;
    Buffers buffers;
    std::vector<ResourceDescription> resourceDescriptions;
    std::vector<ResourceHandle> inputs;
    std::vector<ResourceHandle> outputs;
    ExecutionFunc execute;
};
} // namespace Engine::RenderGraph
