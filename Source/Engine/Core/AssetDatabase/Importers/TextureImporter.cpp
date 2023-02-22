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

    nlohmann::json dummy = nlohmann::json::object();

    size_t byteSize;
    UniPtr<char> data = ReadBinary(path, &byteSize);

    auto output = outputDir / path.filename();
    std::filesystem::copy_file(path, output);

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
    if (!std::filesystem::exists(input))
        return nullptr;

    size_t byteSize;
    UniPtr<char> data = ReadBinary(input, &byteSize);

    UniPtr<Texture> tex = nullptr;
    if (IsKTX1File((uint8_t*)data.Get()) || IsKTX2File((uint8_t*)data.Get()))
    {
        KtxTexture ktxTexture;
        ktxTexture.imageData = (uint8_t*)data.Get();
        ktxTexture.byteSize = byteSize;
        tex = MakeUnique<Texture>(ktxTexture);
    }
    else
    {
        tex = LoadTextureFromBinary((unsigned char*)data.Get(), byteSize);
    }

    tex->SetName(infoJson["name"]);
    tex->SetUUID(uuid);
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
