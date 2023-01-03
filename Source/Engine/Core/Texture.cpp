#include "Texture.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Rendering/GfxResourceTransfer.hpp"

namespace Engine
{
Texture::Texture(TextureDescription texDesc, const UUID& uuid) : AssetObject(uuid)
{
    image = Gfx::GfxDriver::Instance()->CreateImage(texDesc, Gfx::ImageUsage::Texture | Gfx::ImageUsage::TransferDst);
    image->SetName(uuid.ToString());
    Internal::GfxResourceTransfer::ImageTransferRequest request{
        .data = texDesc.data,
        .size = Gfx::Utils::MapImageFormatToByteSize(texDesc.format) * texDesc.width * texDesc.height,
        .keepData = texDesc.keepData
        // use default subresource range
    };
    Internal::GetGfxResourceTransfer()->Transfer(image, request);
}

void Texture::Reload(AssetObject&& loaded)
{
    Texture* newTex = static_cast<Texture*>(&loaded);
    AssetObject::Reload(std::move(loaded));
    image = std::move(newTex->image);
}
} // namespace Engine
