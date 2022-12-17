#include "Texture.hpp"
#include "Rendering/GfxResourceTransfer.hpp"
#include "GfxDriver/GfxEnums.hpp"

namespace Engine
{
    Texture::Texture(TextureDescription texDesc, const UUID& uuid) : AssetObject(uuid)
    {
        image = Gfx::GfxDriver::Instance()->CreateImage(texDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);

        Internal::GfxResourceTransfer::TransferRequest request
        {
            .data = texDesc.data,
            .size = Gfx::Utils::MapImageFormatToByteSize(texDesc.format) * texDesc.width * texDesc.height,
            .onTransferFinished = nullptr
        };
        Internal::GetGfxResourceTransfer()->Transfer(image, request);
    }

    void Texture::Reload(AssetObject&& loaded)
    {
        Texture* newTex = static_cast<Texture*>(&loaded);
        AssetObject::Reload(std::move(loaded));
        image = std::move(newTex->image);
    }
}
