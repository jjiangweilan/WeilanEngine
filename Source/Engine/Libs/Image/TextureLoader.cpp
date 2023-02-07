#include "TextureLoader.hpp"
#include "ThirdParty/stb/stb_image.h"
namespace Libs::Image
{
UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* imageData, std::size_t byteSize)
{
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(imageData, byteSize, &width, &height, &desiredChannels);
    if (desiredChannels == 3)
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
} // namespace Libs::Image
