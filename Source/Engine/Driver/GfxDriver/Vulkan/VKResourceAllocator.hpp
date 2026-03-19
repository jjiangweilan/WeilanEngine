#pragma once
#include "Engine/Driver/GfxDriver/RenderGraph.hpp"
#include "VKImage.hpp"
#include "VKRenderPass.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace Gfx
{
class VKCommandBufferProcessor;

class VKResourceAllocator
{
public:
    VKResourceAllocator(VKCommandBufferProcessor* graph) : graph(graph) {}

    VKImage* GetImage(const UUID& hash);
    VKImage* Request(const ImageIdentifier& id, RenderImageDescriptor& desc);
    VKRenderPass* Request(RenderPass& renderPass);
    void Tick();

private:
    int maxResourceUnusedFrames = 120;

    struct AllocatedImage
    {
        std::unique_ptr<VKImage> image;
        int frameCountFromLastRequest = 0;
        RenderImageDescriptor desc;
    };

    struct AllocatedRenderPass
    {
        std::unique_ptr<VKRenderPass> renderPass;
        std::vector<ObjPtr<Image>> attachments;
        std::vector<ObjPtr<ImageView>> imageViews;
        int frameCountFromLastRequest = 0;

        bool CheckValidationOfAttachments()
        {
            for (auto& r : attachments)
            {
                if (r == nullptr)
                    return false;
            }

            for (auto& v : imageViews)
            {
                if (v == nullptr)
                    return false;
            }

            return true;
        }
    };

    VKCommandBufferProcessor* graph;
    std::unordered_map<UUID, AllocatedImage> images;
    std::unordered_map<RenderPass, AllocatedRenderPass> renderPasses;

    void RemoveImageRelatedInfo(Image* ptr);

    template <class Key, class T>
    void UpdateResources(std::unordered_map<Key, T>& resources)
    {
        for (auto iter = resources.begin(); iter != resources.end();)
        {
            if (iter->second.frameCountFromLastRequest > maxResourceUnusedFrames)
            {
                iter = resources.erase(iter);
            }
            else
            {
                iter->second.frameCountFromLastRequest += 1;
                ++iter;
            }
        }
    }
};

} // namespace Gfx
