#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "AssetObject.hpp"

namespace Engine
{
    using TextureDescription = Gfx::ImageDescription;
    class Texture : public AssetObject
    {
        public:
            Texture(TextureDescription texDesc);
            RefPtr<Gfx::Image> GetGfxImage() { return image; };
        private:
            UniPtr<Gfx::Image> image;
    };
}
