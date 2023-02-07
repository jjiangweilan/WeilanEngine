#include "Texture.hpp"
#include "GfxDriver/GfxEnums.hpp"
#include "Rendering/GfxResourceTransfer.hpp"
#include "ThirdParty/stb/stb_image.h"

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

UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* imageData, std::size_t byteSize)
{
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(imageData, byteSize, &width, &height, &desiredChannels);
    if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
        desiredChannels = 4;

    stbi_uc* loaded = stbi_load_from_memory(imageData, (int)byteSize, &width, &height, &channels, desiredChannels);
    bool is16Bit = stbi_is_16_bit_from_memory(imageData, byteSize);
    bool isHDR = stbi_is_hdr_from_memory(imageData, byteSize);

    if (is16Bit && isHDR)
    {
        SPDLOG_ERROR("16 bits and hdr texture Not Implemented");
    }

    Engine::TextureDescription desc;
    desc.width = width;
    desc.height = height;
    desc.data = loaded;

    if (desiredChannels == 4)
    {
        desc.format = Engine::Gfx::ImageFormat::R8G8B8A8_SRGB;
    }
    if (desiredChannels == 3)
    {
        desc.format = Engine::Gfx::ImageFormat::R8G8B8_SRGB;
    }
    if (desiredChannels == 2)
    {
        desc.format = Engine::Gfx::ImageFormat::R8G8_SRGB;
    }
    if (desiredChannels == 1)
    {
        desc.format = Engine::Gfx::ImageFormat::R8_SRGB;
    }
    return MakeUnique<Engine::Texture>(desc);
}
} // namespace Engine
