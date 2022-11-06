#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "AssetObject.hpp"

namespace Engine
{
    using TextureDescription = Gfx::ImageDescription;
    class Texture : public AssetObject
    {
        public:
            Texture(TextureDescription texDesc, const UUID& uuid = UUID::empty);
            RefPtr<Gfx::Image> GetGfxImage() { return image; };
        private:

            bool Serialize(AssetSerializer&) override{return false;} // disable saving
            UniPtr<Gfx::Image> image;
    };
}
