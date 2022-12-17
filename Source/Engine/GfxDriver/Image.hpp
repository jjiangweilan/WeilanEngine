#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include <cinttypes>
#include <string>
#include "ImageDescription.hpp"

namespace Engine::Gfx
{
    struct ImageSubresourceRange
    {
        ImageAspectFlags    aspectMask;
        uint32_t       baseMipLevel;
        uint32_t       levelCount;
        uint32_t       baseArrayLayer;
        uint32_t       layerCount;
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
