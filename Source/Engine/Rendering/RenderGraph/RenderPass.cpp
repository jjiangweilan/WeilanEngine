#include "RenderPass.hpp"

namespace Engine::RenderGraph
{

const std::vector<Gfx::ImageView*>& RenderPass::GetImageViews(Gfx::Image* image)
{
    return imageToImageViews[image];
}

RenderPass::RenderPass(
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

void RenderPass::Finalize()
{
    renderPass = GetGfxDriver()->CreateRenderPass();

    for (auto& subpass : subpasses)
    {
        std::vector<Gfx::RenderPass::Attachment> colors;

        for (Attachment colorAtta : subpass.colors)
        {
            Gfx::Image* image = (Gfx::Image*)resourceRefs[colorAtta.handle]->GetResource();

            Gfx::ImageView* imageView = nullptr;
            if (colorAtta.imageView.has_value())
            {
                auto newImageView = GetGfxDriver()->CreateImageView({
                    .image = *image,
                    .imageViewType = colorAtta.imageView->imageViewType,
                    .subresourceRange = colorAtta.imageView->subresourceRange,
                });
                imageView = newImageView.get();
                imageViews.push_back(std::move(newImageView));
            }
            else
                imageView = &image->GetDefaultImageView();

            colors.push_back(
                {.imageView = imageView,
                 .multiSampling = colorAtta.multiSampling,
                 .loadOp = colorAtta.loadOp,
                 .storeOp = colorAtta.storeOp,
                 .stencilLoadOp = colorAtta.stencilLoadOp,
                 .stencilStoreOp = colorAtta.stencilStoreOp}
            );
            imageToImageViews[image].push_back(imageView);
        }

        std::optional<Gfx::RenderPass::Attachment> depth = std::nullopt;
        if (subpass.depth.has_value())
        {
            auto depthImage = (Gfx::Image*)resourceRefs[subpass.depth->handle]->GetResource();

            Gfx::ImageView* imageView = nullptr;
            if (subpass.depth->imageView.has_value())
            {
                auto newImageView = GetGfxDriver()->CreateImageView({
                    .image = *depthImage,
                    .imageViewType = subpass.depth->imageView->imageViewType,
                    .subresourceRange = subpass.depth->imageView->subresourceRange,
                });
                imageView = newImageView.get();
                imageViews.push_back(std::move(newImageView));
            }
            else
                imageView = &depthImage->GetDefaultImageView();

            depth = {
                .imageView = imageView,
                .multiSampling = subpass.depth->multiSampling,
                .loadOp = subpass.depth->loadOp,
                .storeOp = subpass.depth->storeOp,
                .stencilLoadOp = subpass.depth->stencilLoadOp,
                .stencilStoreOp = subpass.depth->stencilStoreOp,
            };
            imageToImageViews[depthImage].push_back(imageView);
        }

        renderPass->AddSubpass(colors, depth);
    }
}
} // namespace Engine::RenderGraph
