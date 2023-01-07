#include "TextureImporter.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "Libs/Image/Processor.hpp"
#include "ThirdParty/stb/stb_image.h"
#include "ThirdParty/stb/stb_image_write.h"
#include "Utils/ReadBinary.hpp"

namespace Engine::Internal
{
void TextureImporter::Import(const std::filesystem::path& path,
                             const std::filesystem::path& root,
                             const nlohmann::json& config,
                             const UUID& uuid,
                             const std::unordered_map<std::string, UUID>& containedUUIDs)
{
    auto extension = path.extension();
    auto outputDir = root / "Library" / uuid.ToString();
    if (!std::filesystem::exists(outputDir))
    {
        std::filesystem::create_directory(outputDir);
    }

    const nlohmann::json* actualConfig;
    nlohmann::json dummy = nlohmann::json::object();
    if (config.is_object()) actualConfig = &config;
    else actualConfig = &dummy;
    bool genMipMap = actualConfig->value("genMipMap", false);

    size_t byteSize;
    UniPtr<char> data = ReadBinary(path, &byteSize);
    const stbi_uc* imageData = (const stbi_uc*)data.Get();
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(imageData, byteSize, &width, &height, &desiredChannels);

    if (desiredChannels == 3) desiredChannels = 4;

    stbi_uc* loaded = stbi_load_from_memory(imageData, (int)byteSize, &width, &height, &channels, desiredChannels);

    if (genMipMap)
    {
        if ((width % 2 != 0) || (width % 2 != 0))
        {
            SPDLOG_WARN("Can't generate mipmap when importing {}, not power of two image", path.string());
            goto genMipMapOut;
        }
    }
genMipMapOut:

    auto output = outputDir / path.filename();
    if (extension == ".jpg")
    {
        stbi_write_jpg(output.string().c_str(), width, height, desiredChannels, (const void*)loaded, 0);
    }

    auto outDir = root / "Library" / uuid.ToString();
    nlohmann::json info;
    info["name"] = path.filename();
    std::ofstream outInfo(outDir / "info.json");
    outInfo << info.dump();
}

UniPtr<AssetObject> TextureImporter::Load(const std::filesystem::path& root,
                                          ReferenceResolver& refResolver,
                                          const UUID& uuid)
{
    std::ifstream inInfo(root / "Library" / uuid.ToString() / "info.json");
    auto infoJson = nlohmann::json::parse(inInfo);

    auto input = root / "Library" / uuid.ToString() / infoJson.value("name", "???");
    if (!std::filesystem::exists(input)) return nullptr;

    size_t byteSize;
    UniPtr<char> data = ReadBinary(input, &byteSize);
    const stbi_uc* imageData = (const stbi_uc*)data.Get();
    int width, height, channels, desiredChannels;
    stbi_info_from_memory(imageData, byteSize, &width, &height, &desiredChannels);
    if (desiredChannels == 3) desiredChannels = 4;

    stbi_uc* loaded = stbi_load_from_memory(imageData, (int)byteSize, &width, &height, &channels, desiredChannels);
    bool is16Bit = stbi_is_16_bit_from_memory(imageData, byteSize);
    bool isHDR = stbi_is_hdr_from_memory(imageData, byteSize);

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
    tex->SetName(infoJson["name"]);
    return tex;
}

void TextureImporter::GenerateMipMap(unsigned char* image, int level) {}

const std::type_info& TextureImporter::GetObjectType() { return typeid(Texture); }

const nlohmann::json& TextureImporter::GetDefaultConfig()
{
    static nlohmann::json defaultConfig = R"(
            {
                "genMipMap" : false
            }
        )"_json;

    return defaultConfig;
}
} // namespace Engine::Internal
