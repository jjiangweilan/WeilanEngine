#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include <cinttypes>
#include <string>
#include "ImageDescription.hpp"

namespace Engine::Gfx
{
    struct ImageSubresourceRange
    {
        ImageAspectFlags    aspectMask = ImageAspectFlags::Color;
        uint32_t       baseMipLevel = 0;
        uint32_t       levelCount = 1;
        uint32_t       baseArrayLayer = 0;
        uint32_t       layerCount = 1;
    };
    class Image
    {
        public:
            virtual ~Image() {};
            virtual void SetName(const std::string& name) = 0;
            virtual const ImageDescription& GetDescription() = 0;
        protected:
    };
}
