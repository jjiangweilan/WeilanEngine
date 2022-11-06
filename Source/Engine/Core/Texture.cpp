#include "Texture.hpp"

namespace Engine
{
    Texture::Texture(TextureDescription texDesc, const UUID& uuid) : AssetObject(uuid)
    {
        image = Gfx::GfxDriver::Instance()->CreateImage(texDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);
    }
}
