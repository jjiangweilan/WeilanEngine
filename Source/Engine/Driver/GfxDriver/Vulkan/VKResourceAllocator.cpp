#include "VKResourceAllocator.hpp"
#include "VKCommandBufferProcessor.hpp"
#include "VKImageView.hpp"

namespace Gfx
{

VKImage* VKResourceAllocator::GetImage(const UUID& hash)
{
    auto iter = images.find(hash);
    if (iter != images.end())
    {
        return iter->second.image.get();
    }
    return nullptr;
}

VKImage* VKResourceAllocator::Request(const ImageIdentifier& id, RenderImageDescriptor& desc)
{
    auto iter = images.find(id.GetAsUUID());
    if (iter != images.end() && iter->second.desc == desc)
    {
        iter->second.frameCountFromLastRequest = 0;
        return iter->second.image.get();
    }
    else
    {
        if (iter != images.end())
        {
            RemoveImageRelatedInfo(iter->second.image.get());
            images.erase(iter);
        }

        Gfx::ImageDescription imageDesc;
        imageDesc.width = desc.GetWidth();
        imageDesc.height = desc.GetHeight();
        imageDesc.depth = 1;
        imageDesc.format = desc.GetFormat();
        imageDesc.multiSampling = MultiSampling::Sample_Count_1;
        imageDesc.mipLevels = desc.GetMipLevels();
        imageDesc.isCubemap = false;

        images[id.GetAsUUID()] = {
            std::make_unique<VKImage>(
                imageDesc,
                (Gfx::IsColoFormat(imageDesc.format) ? Gfx::ImageUsage::ColorAttachment
                                                     : Gfx::ImageUsage::DepthStencilAttachment) |
                    Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture |
                    (desc.GetRandomWrite() ? Gfx::ImageUsage::Storage : 0)
            ),
            0,
            desc
        };

        auto& image = images[id.GetAsUUID()].image;
        image->SetName(
            fmt::format(
                "rg-{}-{}",
                id.GetName().empty() ? id.GetAsUUID().ToString() : id.GetName(),
                reinterpret_cast<size_t>(image->GetImage())
            )
        );
        SPDLOG_TRACE(
            "VKCommandBufferProcessor: create new iamge({}) {}",
            reinterpret_cast<size_t>(image.get()),
            id.GetAsUUID().ToString()
        );
        return image.get();
    }
}

VKRenderPass* VKResourceAllocator::Request(RenderPass& renderPass)
{
    auto iter = renderPasses.find(renderPass);
    if (iter != renderPasses.end() && iter->second.CheckValidationOfAttachments())
    {
        iter->second.frameCountFromLastRequest = 0;
        return iter->second.renderPass.get();
    }
    else
    {
        auto renderPassObj = std::make_unique<VKRenderPass>();
        std::vector<ObjPtr<Image>> imageReferences;
        std::vector<ObjPtr<ImageView>> imageViewReferences;

        auto attachments = renderPass.GetAttachments();
        for (auto& subpass : renderPass.GetSubpasses())
        {
            std::vector<Attachment> colors;
            for (const SubpassAttachment& color : subpass.colors)
            {
                const auto& id = attachments[color.attachmentIndex];
                auto idType = id.GetType();
                Gfx::VKImage* image = ImageIdentifier_GetImage(id, graph);
                if (image == nullptr && idType != ImageIdentifier::Type::ImageView)
                {
                    SPDLOG_ERROR("VKResourceAllocator: failed to resolve color attachment image for index {} in render pass {}", color.attachmentIndex, renderPass.GetName());
                    continue;
                }
                Gfx::VKImageView* imageView = idType == ImageIdentifier::Type::ImageView
                                                  ? static_cast<Gfx::VKImageView*>(id.GetAsImageView())
                                                  : static_cast<Gfx::VKImageView*>(&image->GetDefaultImageView());
                imageReferences.push_back(image);

                if (idType == ImageIdentifier::Type::ImageView)
                {
                    imageViewReferences.push_back(imageView);
                }

                colors.push_back(
                    Attachment{
                        imageView,
                        Gfx::MultiSampling::Sample_Count_1,
                        color.loadOp,
                        color.storeOp,
                        color.stencilLoadOp,
                        color.stencilStoreOp,
                    }
                );
            }

            std::optional<Attachment> depth;
            if (subpass.depth.attachmentIndex != -1)
            {
                const auto& id = attachments[subpass.depth.attachmentIndex];
                auto idType = id.GetType();
                Gfx::VKImage* image = ImageIdentifier_GetImage(id, graph);
                if (image == nullptr && idType != ImageIdentifier::Type::ImageView)
                {
                    SPDLOG_ERROR("VKResourceAllocator: failed to resolve depth attachment image for index {} in render pass {}", subpass.depth.attachmentIndex, renderPass.GetName());
                }
                else
                {
                    Gfx::VKImageView* imageView = idType == ImageIdentifier::Type::ImageView
                                                      ? static_cast<Gfx::VKImageView*>(id.GetAsImageView())
                                                      : static_cast<Gfx::VKImageView*>(&image->GetDefaultImageView());
                    imageReferences.push_back(image);

                    if (idType == ImageIdentifier::Type::ImageView)
                    {
                        imageViewReferences.push_back(imageView);
                    }

                    depth = Attachment{
                        imageView,
                        Gfx::MultiSampling::Sample_Count_1,
                        subpass.depth.loadOp,
                        subpass.depth.storeOp,
                        subpass.depth.stencilLoadOp,
                        subpass.depth.stencilStoreOp,
                    };
                }
            }

            renderPassObj->AddSubpass(colors, depth);
        }

        auto temp = renderPassObj.get();
        SPDLOG_TRACE(
            "VKCommandBufferProcessor: create render pass({}) {}",
            reinterpret_cast<size_t>(temp),
            renderPass.GetName()
        );
        renderPasses[renderPass] =
            {std::move(renderPassObj), std::move(imageReferences), std::move(imageViewReferences), 0};

        return temp;
    }
}

void VKResourceAllocator::RemoveImageRelatedInfo(Image* ptr)
{
    graph->RemoveImageRelatedInfo(ptr);
}

void VKResourceAllocator::Tick()
{
    // render pass may depend on allocated image, so we remove renderPass first
    UpdateResources(renderPasses);

    // remove images
    int removeCount = 0;
    UUID readyToRemove[8];
    for (auto& iter : images)
    {
        if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
        {
            readyToRemove[removeCount++] = iter.first;
            RemoveImageRelatedInfo(iter.second.image.get());
        }
        iter.second.frameCountFromLastRequest += 1;
    }

    for (int i = 0; i < removeCount; ++i)
    {
        images.erase(readyToRemove[i]);
    }
}

} // namespace Gfx
