#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/ImageView.hpp"
#include "Resource.hpp"
#include "ResourceCenter.hpp"
#include <functional>
#include <memory>
#include <optional>
namespace RenderGraph
{

using ResourceHandle = size_t;
using ResourceRefs = std::unordered_map<ResourceHandle, ResourceRef*>;

// A high level abstraction of game rendering. For example, Rendering all the opaque objects, rendering shadows,
// velocity pass, TAA pass.
class RenderPass
{
public:
    struct ImageView
    {
        Gfx::ImageViewType imageViewType;
        Gfx::ImageSubresourceRange subresourceRange;
    };

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

        // used when you want to set a barrier to part of the image
        std::optional<Gfx::ImageSubresourceRange> imageSubresourceRange;
    };

    struct Attachment
    {
        ResourceHandle handle;
        std::optional<ImageView> imageView;
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
    );
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

    const std::vector<Gfx::ImageView*>& GetImageViews(Gfx::Image* image);

    // Recording all the gfx commands.
    // Don't insert any pipeline barriers for resources from outside, it's managed by the frame
    virtual void Execute(Gfx::CommandBuffer& cmdBuf)
    {
        if (execute)
            execute(cmdBuf, *renderPass, resourceRefs);
    };

    // call when all the resources are set
    void Finalize();

protected:
    std::vector<ResourceHandle> externalResources;
    std::unique_ptr<Gfx::RenderPass> renderPass = nullptr;
    std::vector<ResourceHandle> creationRequests;
    std::vector<std::unique_ptr<Gfx::ImageView>> imageViews;

    // it's possition a image is read from a subresource and write to another subresource
    // this is used to enable finer control over Graph's barrier insertion
    std::unordered_map<Gfx::Image*, std::vector<Gfx::ImageView*>> imageToImageViews;
    const std::vector<Subpass> subpasses;
    ResourceRefs resourceRefs;
    std::unordered_map<ResourceHandle, ResourceDescription> resourceDescriptions;
    ExecutionFunc execute;
};
} // namespace RenderGraph
