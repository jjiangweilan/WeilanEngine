#include "TextureImporter.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "ThirdParty/stb/stb_image.h"
#include "Utils/ReadBinary.hpp"
namespace Engine::Internal
{
    UniPtr<AssetObject> TextureImporter::Import(
            const std::filesystem::path& path,
            const nlohmann::json& config,
            ReferenceResolver& refResolver,
            const UUID& uuid, const std::unordered_map<std::string, UUID>& containedUUIDs)
    {
        // read config
        bool genMipMap = config.value("genMipMap", false);

        size_t byteSize;
        UniPtr<char> data = ReadBinary(path, &byteSize);
        const stbi_uc* dataCast = (const stbi_uc*)data.Get();
        int width, height, channels, desiredChannels;
        stbi_info_from_memory(dataCast, byteSize, &width, &height, &desiredChannels);
        if (desiredChannels == 3) desiredChannels = 4;

        stbi_uc* loaded = stbi_load_from_memory(dataCast, (int)byteSize, &width, &height, &channels, desiredChannels);
        bool is16Bit = stbi_is_16_bit_from_memory(dataCast, byteSize);
        bool isHDR = stbi_is_hdr_from_memory(dataCast, byteSize);

        if (is16Bit && isHDR)
        {
            SPDLOG_ERROR("16 bits and hdr texture Not Implemented");
        }

        TextureDescription desc;
        desc.width = width;
        desc.height = height;
        desc.data = loaded;

        if (desiredChannels == 4)
        {
            desc.format = Gfx::ImageFormat::R8G8B8A8_SRGB;
        }
        if (desiredChannels == 3)
        {
            desc.format = Gfx::ImageFormat::R8G8B8_SRGB;
        }
        if (desiredChannels == 2)
        {
            desc.format = Gfx::ImageFormat::R8G8_SRGB;
        }
        if (desiredChannels == 1)
        {
            desc.format = Gfx::ImageFormat::R8_SRGB;
        }

        auto tex = MakeUnique<Texture>(desc, uuid);
        tex->SetName(path.filename().string());
        delete loaded;
        return tex;
    }

    nlohmann::json TextureImporter::GetDefaultConfig()
    {
        return nlohmann::json::parse(R"(
        {
            "genMipMap": false,
        }
        )");
    }
}
