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

    // if this is a creational description, imageCreateInfo or bufferCreationInfo should be filled respectively
    struct ResourceDescription
    {
        std::string name;
        ResourceHandle handle;
        ResourceType type;

        Gfx::AccessMaskFlags accessFlags;
        Gfx::PipelineStageFlags stageFlags;
        union
        {
            Gfx::BufferUsageFlags bufferUsageFlags;
            Gfx::ImageUsageFlags imageUsagesFlags;
        };
        Gfx::ImageLayout imageLayout;

        // creational info
        union
        {
            Gfx::ImageDescription imageCreateInfo;
            Gfx::Buffer::CreateInfo bufferCreateInfo;
        };

        Gfx::Image* externalImage;
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
    using ResourceRefs = std::unordered_map<ResourceHandle, ResourceRef*>;
    using Buffers = std::unordered_map<ResourceHandle, ResourceRef*>;
    using ExecutionFunc = std::function<void(CommandBuffer&, Gfx::RenderPass& renderPass, const Buffers& buffers)>;

    RenderPass(
        const ExecutionFunc& execute,
        const std::vector<ResourceDescription>& resourceDescs,
        const std::vector<ResourceHandle>& creationRequests,
        const std::vector<ResourceHandle>& inputs,
        const std::vector<ResourceHandle>& outputs,
        const std::vector<Subpass>& subpasses
    )
        : creationRequests(creationRequests), inputs(inputs), outputs(outputs), subpasses(subpasses), execute(execute)
    {
        for (const ResourceDescription& res : resourceDescs)
        {
            resourceDescriptions[res.handle] = res;

            if (res.externalImage != nullptr)
            {
                externalResources.push_back(res.handle);
            }
        }

        resourceRefs.reserve(resourceDescs.size());
    }
    ~RenderPass() {}
    // Set resource that is needed by this node.framgraph
    void SetResourceRef(ResourceHandle handle, ResourceRef* resource)
    {
        resourceRefs[handle] = resource;
    }

    // Get a resource from the RenderPass.
    ResourceRef* GetResourceRef(ResourceHandle handle)
    {
        return resourceRefs.at(handle);
    }

    // Get ResourceCreationRequests, a ResourceCreationRequests describe
    virtual std::span<ResourceHandle> GetResourceCreationRequests()
    {
        return creationRequests;
    }

    const auto& GetResourceDescriptions()
    {
        return resourceDescriptions;
    }

    virtual const ResourceDescription& GetResourceDescription(ResourceHandle handle)
    {
        return resourceDescriptions[handle];
    }

    // Get resource inputs, use GetReferenceResourceDescription to retrive information about this resource
    std::span<const ResourceHandle> GetResourceInputs()
    {
        return inputs;
    }

    // Get resource outputs, use GetReferenceResourceDescription to retrive information about this resource
    std::span<const ResourceHandle> GetResourceOutputs()
    {
        return outputs;
    }

    std::span<const ResourceHandle> GetExternalResources()
    {
        return externalResources;
    }

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

            for (Attachment colorAtta : subpass.colors)
            {
                Gfx::Image* image = (Gfx::Image*)resourceRefs[colorAtta.handle]->GetResource();
                colors.push_back(
                    {.image = image,
                     .multiSampling = colorAtta.multiSampling,
                     .loadOp = colorAtta.loadOp,
                     .storeOp = colorAtta.storeOp,
                     .stencilLoadOp = colorAtta.stencilLoadOp,
                     .stencilStoreOp = colorAtta.stencilStoreOp}
                );
            }

            if (subpass.depth.has_value())
            {
                depth->image = (Gfx::Image*)resourceRefs[subpass.depth->handle]->GetResource();
                depth->multiSampling = subpass.depth->multiSampling;
                depth->loadOp = subpass.depth->loadOp;
                depth->storeOp = subpass.depth->storeOp;
                depth->stencilLoadOp = subpass.depth->stencilLoadOp;
                depth->stencilStoreOp = subpass.depth->stencilStoreOp;
            }

            renderPass->AddSubpass(colors, depth);
        }
        for (auto& r : resourceRefs)
        {
            if (r.second->IsType(ResourceType::Buffer))
            {
                buffers[r.first] = r.second;
            }
        }
    }

protected:
    std::vector<ResourceHandle> externalResources;
    std::unique_ptr<Gfx::RenderPass> renderPass;
    std::vector<ResourceHandle> creationRequests;
    const std::vector<Subpass> subpasses;
    ResourceRefs resourceRefs;
    Buffers buffers;
    std::unordered_map<ResourceHandle, ResourceDescription> resourceDescriptions;
    std::vector<ResourceHandle> inputs;
    std::vector<ResourceHandle> outputs;
    ExecutionFunc execute;
};
} // namespace Engine::RenderGraph
