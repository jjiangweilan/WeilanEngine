#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "RenderPass.hpp"
#include "ResourceCenter.hpp"
#include <functional>
#include <memory>
namespace Engine::RenderGraph
{

using ResourceHandle = int;
using ResourceRefs = std::unordered_map<ResourceHandle, ResourceRef*>;

// A high level abstraction of game rendering. For example, Rendering all the opaque objects, rendering shadows,
// velocity pass, TAA pass.
class RenderPass
{
public:
    // if this is a creational description, imageCreateInfo or bufferCreationInfo should be filled respectively
    struct ResourceDescription
    {
        std::string name;
        ResourceHandle handle;
        ResourceType type;

        Gfx::AccessMaskFlags accessFlags;
        Gfx::PipelineStageFlags stageFlags;
        Gfx::ImageUsageFlags imageUsagesFlags;
        Gfx::ImageLayout imageLayout;

        // creational info
        union
        {
            Gfx::ImageDescription imageCreateInfo;
            Gfx::Buffer::CreateInfo bufferCreateInfo;
        };

        Gfx::Image* externalImage;
        Gfx::Buffer* externalBuffer;
    };

    struct Attachment
    {
        ResourceHandle handle;
        Gfx::MultiSampling multiSampling = Gfx::MultiSampling::Sample_Count_1;
        Gfx::AttachmentLoadOperation loadOp = Gfx::AttachmentLoadOperation::Clear;
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
    using PassResources = std::unordered_map<ResourceHandle, ResourceRef>;
    using ExecutionFunc = std::function<void(Gfx::CommandBuffer&, Gfx::RenderPass&, const ResourceRefs&)>;

    RenderPass(
        const ExecutionFunc& execute,
        const std::vector<ResourceDescription>& resourceDescs,
        const std::vector<Subpass>& subpasses
    )
        : subpasses(subpasses), execute(execute)
    {
        for (ResourceDescription res : resourceDescs)
        {
            if (res.externalImage != nullptr)
            {
                externalResources.push_back(res.handle);
                res.imageCreateInfo = res.externalImage->GetDescription();
            }

            if (res.externalBuffer != nullptr)
            {
                externalResources.push_back(res.handle);
            }

            if (res.externalImage == nullptr && res.type == ResourceType::Image && res.imageCreateInfo.width != 0 &&
                res.imageCreateInfo.height != 0)
            {
                creationRequests.push_back(res.handle);
            }
            else if (res.externalBuffer == nullptr && res.type == ResourceType::Buffer && res.bufferCreateInfo.size != 0)
            {
                creationRequests.push_back(res.handle);
            }

            resourceDescriptions[res.handle] = res;
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

    bool HasResourceDescription(ResourceHandle handle)
    {
        return resourceDescriptions.contains(handle);
    }

    const auto& GetResourceDescriptions()
    {
        return resourceDescriptions;
    }

    virtual const ResourceDescription& GetResourceDescription(ResourceHandle handle)
    {
        return resourceDescriptions[handle];
    }

    std::span<const ResourceHandle> GetExternalResources()
    {
        return externalResources;
    }

    // Recording all the gfx commands.
    // Don't insert any pipeline barriers for resources from outside, it's managed by the frame
    virtual void Execute(Gfx::CommandBuffer& cmdBuf)
    {
        if (execute)
            execute(cmdBuf, *renderPass, resourceRefs);
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
                depth = Gfx::RenderPass::Attachment();
                depth->image = (Gfx::Image*)resourceRefs[subpass.depth->handle]->GetResource();
                depth->multiSampling = subpass.depth->multiSampling;
                depth->loadOp = subpass.depth->loadOp;
                depth->storeOp = subpass.depth->storeOp;
                depth->stencilLoadOp = subpass.depth->stencilLoadOp;
                depth->stencilStoreOp = subpass.depth->stencilStoreOp;
            }

            renderPass->AddSubpass(colors, depth);
        }
    }

protected:
    std::vector<ResourceHandle> externalResources;
    std::unique_ptr<Gfx::RenderPass> renderPass;
    std::vector<ResourceHandle> creationRequests;
    const std::vector<Subpass> subpasses;
    ResourceRefs resourceRefs;
    std::unordered_map<ResourceHandle, ResourceDescription> resourceDescriptions;
    ExecutionFunc execute;
};
} // namespace Engine::RenderGraph
