#include "Texture.hpp"

namespace Engine
{
    Texture::Texture(TextureDescription texDesc, const UUID& uuid) : AssetObject(uuid)
    {
        image = Gfx::GfxDriver::Instance()->CreateImage(texDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);
    }

    void Texture::Reload(AssetObject&& loaded)
    {
        Texture* newTex = static_cast<Texture*>(&loaded);
        AssetObject::Reload(std::move(loaded));
        image = std::move(newTex->image);
    }
}
