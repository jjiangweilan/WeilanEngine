#include "TextureImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/ArtifactTypes.hpp"
#include "Engine/Runtime/System/AssetDatabase/Exporters/KtxExporter.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Engine/Library/Image/ImageProcessing.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/ThirdParty/stb/stb_image.h"
#include "Engine/ThirdParty/xxHash/xxhash.h"
#include <fstream>
#include <functional>
#include <ktx.h>
#include <ktxvulkan.h>
#include <limits>
#include <optional>
#include <spdlog/spdlog.h>
#include <string_view>

namespace
{
uint64_t ComputeMetaHash(const nlohmann::json& meta)
{
    return std::hash<std::string>{}(meta.value("importOption", nlohmann::json::object()).dump());
}

uint64_t ComputeContentHash(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.good())
    {
        return 0;
    }

    const auto fileSize = std::filesystem::file_size(path);
    std::vector<char> fileData(fileSize);
    f.read(fileData.data(), static_cast<std::streamsize>(fileSize));
    return XXH3_64bits(fileData.data(), fileData.size());
}

struct KtxColorDescription
{
    bool srgb = false;
    uint32_t levels = 1;
};

bool IsVkSRGBFormat(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return true;
        default: return false;
    }
}

std::optional<KtxColorDescription> ReadKtxColorDescription(const std::filesystem::path& path)
{
    ktxTexture2* texture = nullptr;
    const KTX_error_code result =
        ktxTexture2_CreateFromNamedFile(path.string().c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &texture);
    if (result != KTX_SUCCESS)
    {
        spdlog::error("failed to inspect KTX texture {}: {}", path.string(), ktxErrorString(result));
        return std::nullopt;
    }

    const khr_df_transfer_e transfer = ktxTexture2_GetOETF_e(texture);
    const bool dfdSRGB = transfer == KHR_DF_TRANSFER_SRGB;
    const bool dfdKnown = dfdSRGB || transfer == KHR_DF_TRANSFER_LINEAR;
    const VkFormat vkFormat = ktxTexture2_GetVkFormat(texture);
    const bool formatKnown = vkFormat != VK_FORMAT_UNDEFINED;
    const bool formatSRGB = formatKnown && IsVkSRGBFormat(vkFormat);

    if (formatKnown && dfdKnown && formatSRGB != dfdSRGB)
    {
        spdlog::warn(
            "KTX texture {} has conflicting Vulkan format and DFD transfer function; Vulkan format controls sampling",
            path.string()
        );
    }

    KtxColorDescription description{
        .srgb = formatKnown ? formatSRGB : dfdSRGB,
        .levels = texture->numLevels,
    };
    ktxTexture_Destroy(ktxTexture(texture));
    return description;
}
} // namespace

void StbiDeleter(uint8_t* p)
{
    stbi_image_free(p);
}

void NewDeleter(uint8_t* p)
{
    delete[] p;
}

using UniqueImagePtr = std::unique_ptr<uint8_t[], void (*)(uint8_t*)>;

DEFINE_ASSET_IMPORTER(TextureImporter, "ktx2,ktx,jpg,png,jpeg,bmp,hdr,psd,tga,gif,pic,pgm,ppm");

TextureImporter::TextureImporter() = default;
TextureImporter::~TextureImporter() = default;

const std::vector<std::type_index>& TextureImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Texture)};
    return types;
}

bool TextureImporter::ImportNeeded()
{
    ImportDatabase::ImportState state{};
    std::filesystem::path artifactPath;
    const auto currentWriteTime = static_cast<uint64_t>(std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count());
    const auto currentMetaHash = ComputeMetaHash(meta);
    const auto currentContentHash = ComputeContentHash(absoluteAssetPath);

    if (!importDatabase->TryGetImportState(assetUUID.ToString(), state))
    {
        return true;
    }

    if (!importDatabase->TryGetArtifactPath(assetUUID.ToString(), AssetArtifacts::Kind::Texture, artifactPath))
    {
        return true;
    }

    return state.sourceWriteTime != currentWriteTime || state.metaHash != currentMetaHash ||
           state.contentHash != currentContentHash ||
           !importDatabase->ArtifactExists(artifactPath);
}

std::vector<std::filesystem::path> TextureImporter::Import()
{
    auto IsLinearFormat = [this](bool is16Bit, bool isHDR)
    {
        if (is16Bit || isHDR)
            return true;

        bool linearFormat = true;
        auto lowerCasePathStr = Utils::strToLower(absoluteAssetPath.filename().string());

        bool srgFormat =
            Utils::strContians(lowerCasePathStr, "srgb") || Utils::strContians(lowerCasePathStr, "diffuse") ||
            Utils::strContians(lowerCasePathStr, "albedo") || Utils::strContians(lowerCasePathStr, "basecolor");
        linearFormat = !srgFormat;

        return linearFormat;
    };

    nlohmann::json option = meta.value("importOption", nlohmann::json::object_t{});
    bool generateMipmap = option.value("generateMipmap", true);
    bool converToIrradianceCubemap = option.value("convertToIrradianceCubemap", false);
    bool convertToReflectanceCubemap = option.value("convertToReflectanceCubemap", false);
    bool convertToCubemap = option.value("convertToCubemap", false);
    bool linearFormat = true;
    if (convertToCubemap)
    {
        converToIrradianceCubemap = false;
        convertToReflectanceCubemap = false;
    }

    if (convertToReflectanceCubemap)
    {
        generateMipmap = false;
    }

    const std::string extension = Utils::strToLower(absoluteAssetPath.extension().string());
    const bool isKtx = extension == ".ktx" || extension == ".ktx2";

    std::filesystem::path importedAssetPath;
    if (!importDatabase->TryGetArtifactPath(assetUUID.ToString(), AssetArtifacts::Kind::Texture, importedAssetPath))
    {
        importedAssetPath = importDatabase->GetImportAssetPath(UUID().ToString()).replace_extension(AssetArtifacts::Extension(AssetArtifacts::Kind::Texture));
    }

    std::ifstream f(absoluteAssetPath, std::ios::binary);
    if (!f.good())
    {
        spdlog::error("failed to open texture source {}", absoluteAssetPath.string());
        return {};
    }

    if (!isKtx)
    {
        std::error_code fileSizeError;
        uintmax_t fileSize = std::filesystem::file_size(absoluteAssetPath, fileSizeError);
        if (fileSizeError || fileSize == 0 || fileSize > static_cast<uintmax_t>(std::numeric_limits<int>::max()))
        {
            spdlog::error("invalid texture source size for {}", absoluteAssetPath.string());
            return {};
        }

        std::vector<char> fileData(static_cast<size_t>(fileSize));
        f.read(fileData.data(), static_cast<std::streamsize>(fileSize));
        if (!f)
        {
            spdlog::error("failed to read texture source {}", absoluteAssetPath.string());
            return {};
        }

        uint8_t* data = reinterpret_cast<uint8_t*>(fileData.data());
        int byteSize = static_cast<int>(fileSize);
        int width = 0;
        int height = 0;
        int channels = 0;
        int desiredChannels = 0;
        bool isCubemap = (convertToReflectanceCubemap || converToIrradianceCubemap || convertToCubemap);
        int layers = isCubemap ? 6 : 1;
        if (stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels) == 0 || width <= 0 ||
            height <= 0 || desiredChannels <= 0 || desiredChannels > 4)
        {
            spdlog::error(
                "failed to inspect texture source {}: {}",
                absoluteAssetPath.string(),
                stbi_failure_reason() ? stbi_failure_reason() : "unknown stb error"
            );
            return {};
        }

        if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
            desiredChannels = 4;

        bool is16Bit = stbi_is_16_bit_from_memory(data, byteSize);
        bool isHDR = stbi_is_hdr_from_memory(data, byteSize);

        std::string colorSpace;
        if (option.contains("colorSpace"))
        {
            colorSpace = option.value("colorSpace", "linear");
            if (colorSpace != "linear" && colorSpace != "srgb")
            {
                spdlog::warn("invalid colorSpace '{}' for {}; inferring from texture semantics", colorSpace, absoluteAssetPath.string());
                colorSpace = IsLinearFormat(is16Bit, isHDR) ? "linear" : "srgb";
            }
        }
        else if (option.contains("linearFormat"))
            colorSpace = option.value("linearFormat", true) ? "linear" : "srgb";
        else
            colorSpace = IsLinearFormat(is16Bit, isHDR) ? "linear" : "srgb";
        if ((is16Bit || isHDR) && colorSpace == "srgb")
        {
            spdlog::warn(
                "texture {} uses a high-precision format without an sRGB Vulkan variant; importing as linear",
                absoluteAssetPath.string()
            );
            colorSpace = "linear";
        }
        linearFormat = colorSpace == "linear";

        int mipLevels = generateMipmap ? glm::floor(glm::log2((float)glm::min(width, height))) + 1 : 1;

        UniqueImagePtr loaded(nullptr, StbiDeleter);
        if (isHDR)
        {
            loaded.reset(
                reinterpret_cast<uint8_t*>(
                    stbi_loadf_from_memory(data, byteSize, &width, &height, &channels, desiredChannels)
                )
            );
        }
        else if (is16Bit)
        {
            loaded.reset(
                reinterpret_cast<uint8_t*>(
                    stbi_load_16_from_memory(data, byteSize, &width, &height, &channels, desiredChannels)
                )
            );
        }
        else
        {
            loaded.reset(stbi_load_from_memory(data, byteSize, &width, &height, &channels, desiredChannels));
        }

        if (loaded == nullptr)
        {
            spdlog::error(
                "failed to decode texture source {}: {}",
                absoluteAssetPath.string(),
                stbi_failure_reason() ? stbi_failure_reason() : "unknown stb error"
            );
            return {};
        }

        if (converToIrradianceCubemap)
        {
            uint8_t* output;
            int cubemapSize = 1024;
            Libs::Image::GenerateIrradianceCubemap((float*)loaded.get(), width, height, cubemapSize, output);
            loaded = UniqueImagePtr(output, NewDeleter);
            width = cubemapSize;
            height = cubemapSize;
        }

        if (convertToCubemap)
        {
            uint8_t* output;
            int cubemapSize = 1024;
            Libs::Image::ConverToCubemap((float*)loaded.get(), width, height, cubemapSize, desiredChannels, output);
            loaded = UniqueImagePtr(output, NewDeleter);
            width = cubemapSize;
            height = cubemapSize;
        }

        if (convertToReflectanceCubemap)
        {
            uint8_t* output;
            int cubemapSize = 1024;
            Libs::Image::GenerateReflectanceCubemap((float*)loaded.get(), width, height, cubemapSize, output, mipLevels);
            loaded = UniqueImagePtr(output, NewDeleter);
            width = cubemapSize;
            height = cubemapSize;
        }

        if (generateMipmap)
        {
            size_t mippedDataByteSize = 0;
            uint8_t* mippedData = nullptr;
            if (isHDR)
            {
                Libs::Image::GenerateBoxFilteredMipmap<float>(
                    loaded.get(), width, height, layers, mipLevels, desiredChannels, mippedData, mippedDataByteSize
                );
            }
            else if (is16Bit)
            {
                Libs::Image::GenerateBoxFilteredMipmap<uint16_t>(
                    loaded.get(), width, height, layers, mipLevels, desiredChannels, mippedData, mippedDataByteSize
                );
            }
            else
            {
                if (linearFormat)
                    Libs::Image::GenerateBoxFilteredMipmap<uint8_t>(
                        loaded.get(), width, height, layers, mipLevels, desiredChannels, mippedData, mippedDataByteSize
                    );
                else
                    Libs::Image::GenerateBoxFilteredMipmapSRGB8(
                        loaded.get(), width, height, layers, mipLevels, desiredChannels, mippedData, mippedDataByteSize
                    );
            }
            loaded = UniqueImagePtr(mippedData, NewDeleter);
        }

        int bits = 8;
        if (isHDR)
            bits = 32;
        if (is16Bit)
            bits = 16;

        Gfx::GfxFormat format = Gfx::GetGfxFormat(bits, desiredChannels, linearFormat);

        Exporters::KtxExporter::Export(
            (importDatabase->GetImportDatabaseRootPath() / std::filesystem::path(importedAssetPath.string()))
                .string()
                .c_str(),
            loaded.get(),
            width,
            height,
            1,
            2,
            isCubemap ? 1 : layers,
            mipLevels,
            false,
            isCubemap,
            format,
            true
        );
    }
    else
    {
        const auto ktxDescription = ReadKtxColorDescription(absoluteAssetPath);
        if (!ktxDescription)
            return {};

        linearFormat = !ktxDescription->srgb;
        generateMipmap = ktxDescription->levels > 1;
        std::optional<bool> metadataSRGB;
        if (option.contains("colorSpace"))
            metadataSRGB = option.value("colorSpace", "linear") == "srgb";
        else if (option.contains("linearFormat"))
            metadataSRGB = !option.value("linearFormat", true);
        if (metadataSRGB && *metadataSRGB != ktxDescription->srgb)
        {
            spdlog::warn(
                "texture metadata color space disagrees with KTX header for {}; the KTX header is authoritative",
                absoluteAssetPath.string()
            );
        }

        std::ofstream outf(
            importDatabase->GetImportDatabaseRootPath() / importedAssetPath,
            std::ios::trunc | std::ios::out | std::ios::binary
        );
        if (!outf.good())
        {
            spdlog::error("failed to create imported texture artifact {}", importedAssetPath.string());
            return {};
        }
        outf << f.rdbuf();
        if (!outf.good())
        {
            spdlog::error("failed to copy texture artifact {}", importedAssetPath.string());
            return {};
        }
    }
    option.erase("linearFormat");
    option["colorSpace"] = linearFormat ? "linear" : "srgb";
    option["generateMipmap"] = generateMipmap;
    option["convertToIrradianceCubemap"] = converToIrradianceCubemap;
    option["convertToReflectanceCubemap"] = convertToReflectanceCubemap;
    option["convertToCubemap"] = convertToCubemap;
    meta.erase("lastImportedWriteTime");
    meta.erase("importedKtxFile");
    meta.erase("importFileUUID");
    meta["importOption"] = option;

    importDatabase->UpsertImportState(
        assetUUID.ToString(),
        ImportDatabase::ImportState{
            static_cast<uint64_t>(std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count()),
            ComputeMetaHash(meta),
            ComputeContentHash(absoluteAssetPath)
        }
    );
    importDatabase->ReplaceArtifact(assetUUID.ToString(), AssetArtifacts::Kind::Texture, importedAssetPath);

    return {};
}
