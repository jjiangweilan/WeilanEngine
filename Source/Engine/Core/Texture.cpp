#include "Texture.hpp"

namespace Engine
{
    Texture::Texture(TextureDescription texDesc)
    {
        image = Gfx::GfxDriver::Instance()->CreateImage(texDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);
    }
}
