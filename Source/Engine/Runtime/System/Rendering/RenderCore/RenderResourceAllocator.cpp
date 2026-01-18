#include "RenderResourceAllocator.hpp"

Gfx::Image* RenderResourceAllocator::GetImage(const UUID& hash)
{
    auto iter = images.find(hash);
    if (iter != images.end())
    {
        return iter->second.image.get();
    }
    return nullptr;
}

Gfx::Image* RenderResourceAllocator::Request(const Gfx::ImageIdentifier& id, const Gfx::RenderImageDescriptor& desc)
{
    const auto& uuid = id.GetAsUUID();
    auto iter = images.find(uuid);
    if (iter != images.end() && iter->second.desc == desc)
    {
        iter->second.frameCountFromLastRequest = 0;
        return iter->second.image.get();
    }
    else
    {
        if (iter != images.end())
        {
            images.erase(iter);
        }

        Gfx::ImageDescription imageDesc;
        imageDesc.width = desc.GetWidth();
        imageDesc.height = desc.GetHeight();
        imageDesc.depth = 1;
        imageDesc.format = desc.GetFormat();
        imageDesc.multiSampling = Gfx::MultiSampling::Sample_Count_1;
        imageDesc.mipLevels = 1;
        imageDesc.isCubemap = false;

        images[uuid] = {
            GetGfxDriver()->CreateImage(
                imageDesc,
                (Gfx::IsColoFormat(imageDesc.format) ? Gfx::ImageUsage::ColorAttachment
                                                     : Gfx::ImageUsage::DepthStencilAttachment) |
                    Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::TransferSrc | Gfx::ImageUsage::Texture |
                    (desc.GetRandomWrite() ? Gfx::ImageUsage::Storage : 0)
            ),
            0,
            desc
        };

        const auto& uuidStr = uuid.ToString();
        const auto& idName = id.GetName();
        auto& image = images[uuid].image;
        image->SetName(
            fmt::format(
                "rg-{}-{}",
                idName.empty() ? uuidStr : idName,
                reinterpret_cast<size_t>(image.get())
            )
        );
        SPDLOG_TRACE(
            "VKCommandBufferProcessor: create new iamge({}) {}",
            reinterpret_cast<size_t>(image.get()),
            uuidStr
        );
        return image.get();
    }
}

void RenderResourceAllocator::UpdateUnusedFrames()
{
    int removeCount = 0;
    UUID readyToRemove[8];
    for (auto& iter : images)
    {
        if (iter.second.frameCountFromLastRequest > maxResourceUnusedFrames && removeCount < 8)
        {
            readyToRemove[removeCount++] = iter.first;
        }
        iter.second.frameCountFromLastRequest += 1;
    }

    for (int i = 0; i < removeCount; ++i)
    {
        images.erase(readyToRemove[i]);
    }
}
