#pragma once
#include "GfxDriver/GfxEnums.hpp"

#include <cinttypes>
#include <string>
#include "ImageDescription.hpp"

namespace Engine::Gfx
{
    class Image
    {
        public:
            virtual ~Image() {};
            virtual const ImageDescription& GetDescription() = 0;
        protected:
    };
}
