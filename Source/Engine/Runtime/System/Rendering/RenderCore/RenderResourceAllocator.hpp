#pragma once

#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Library/UUID.hpp"
#include "RenderCoreData.hpp"

class RenderResourceAllocator
{
public:
    Gfx::Image* GetImage(const UUID& hash);
    Gfx::Image* Request(const Gfx::ImageIdentifier& id, const Gfx::RenderImageDescriptor& desc);
    void UpdateUnusedFrames();

private:
    int maxResourceUnusedFrames = 8;

    struct AllocatedImage
    {
        std::unique_ptr<Gfx::Image> image;
        int frameCountFromLastRequest = 0;
        Gfx::RenderImageDescriptor desc;
    };

    std::unordered_map<UUID, AllocatedImage> images;
};
