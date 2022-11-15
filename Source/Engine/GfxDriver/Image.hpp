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
            virtual void SetName(const std::string& name) = 0;
            virtual const ImageDescription& GetDescription() = 0;
        protected:
    };
}
