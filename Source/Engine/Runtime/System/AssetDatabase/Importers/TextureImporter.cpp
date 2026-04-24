#include "TextureImporter.hpp"
#include "Engine/Runtime/System/AssetDatabase/Exporters/KtxExporter.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Driver/GfxDriver/Image.hpp"
#include "Engine/Driver/GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Engine/Library/Image/ImageProcessing.hpp"
#include "Engine/Library/UUID.hpp"
#include "Engine/Library/Utils.hpp"
#include "Engine/ThirdParty/stb/stb_image.h"
#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>

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

const std::vector<std::type_index>& TextureImporter::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Texture)};
    return types;
}

bool TextureImporter::ImportNeeded()
{
    size_t oldLastWriteTime = meta.value("lastImportedWriteTime", 0ll);
    size_t lastWriteTime = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    bool reimport = lastWriteTime > oldLastWriteTime;

    // meta file validation
    if (meta.contains("importedKtxFile"))
    {
        reimport = reimport || !importDatabase->ExistImportFile(std::string(meta["importedKtxFile"]));
    }
    else
        reimport = true;

    return reimport;
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

    std::string importFileUUID = meta.value("importFileUUID", UUID().ToString());
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

    auto importedAssetPath = importDatabase->GetImportAssetPath(importFileUUID).replace_extension(".ktx");

    std::fstream f;
    f.open(absoluteAssetPath, std::ios::binary | std::ios_base::in);
    if (f.good() && f.is_open())
    {
        if (absoluteAssetPath.extension() != ".ktx" && absoluteAssetPath.extension() != ".ktx2")
        {
            size_t fileSize = std::filesystem::file_size(absoluteAssetPath);
            std::vector<char> fileData(fileSize);
            f.read(fileData.data(), fileSize);

            uint8_t* data = (uint8_t*)fileData.data();
            size_t byteSize = fileSize;
            int width, height, channels, desiredChannels;
            bool isCubemap = (convertToReflectanceCubemap || converToIrradianceCubemap || convertToCubemap);
            int layers = isCubemap ? 6 : 1;
            stbi_info_from_memory(data, byteSize, &width, &height, &desiredChannels);
            if (desiredChannels == 3) // 3 channel srgb texture is not supported on PC
                desiredChannels = 4;
            // image information
            bool is16Bit = stbi_is_16_bit_from_memory(data, byteSize);
            bool isHDR = stbi_is_hdr_from_memory(data, byteSize);

            if (!option.contains("linearFormat"))
            {
                linearFormat = IsLinearFormat(is16Bit, isHDR);
            }
            else
                linearFormat = option.value("linearFormat", true);

            int mipLevels = generateMipmap ? glm::floor(glm::log2((float)glm::min(width, height))) + 1 : 1;

            UniqueImagePtr loaded(nullptr, StbiDeleter);
            if (isHDR)
            {
                loaded.reset(
                    (uint8_t*)stbi_loadf_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels)
                );
            }
            else if (is16Bit)
            {
                loaded.reset((uint8_t*)stbi_load_16_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels)
                );
            }
            else
            {
                loaded.reset(stbi_load_from_memory(data, (int)byteSize, &width, &height, &channels, desiredChannels));
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
                        loaded.get(),
                        width,
                        height,
                        layers,
                        mipLevels,
                        desiredChannels,
                        mippedData,
                        mippedDataByteSize
                    );
                    loaded = UniqueImagePtr(mippedData, NewDeleter);
                }
                else if (is16Bit)
                {
                    Libs::Image::GenerateBoxFilteredMipmap<uint16_t>(
                        loaded.get(),
                        width,
                        height,
                        layers,
                        mipLevels,
                        desiredChannels,
                        mippedData,
                        mippedDataByteSize
                    );
                    loaded = UniqueImagePtr(mippedData, NewDeleter);
                }
                else
                {
                    Libs::Image::GenerateBoxFilteredMipmap<uint8_t>(
                        loaded.get(),
                        width,
                        height,
                        layers,
                        mipLevels,
                        desiredChannels,
                        mippedData,
                        mippedDataByteSize
                    );
                    loaded = UniqueImagePtr(mippedData, NewDeleter);
                }
            }

            auto filename = absoluteAssetPath.filename().string();
            int bits = 8;
            if (isHDR)
                bits = 32;
            if (is16Bit)
                bits = 16;

            Gfx::GfxFormat format = Gfx::GetGfxFormat(bits, desiredChannels, linearFormat);

            //
            // note: this "converToCube ? 1 : layers" prevents creating array of cubeMaps, but ktx separate the
            // concept of face and layer, that's why I need to manually convert layer to face
            Exporters::KtxExporter::Export(
                (importDatabase->GetImportDatabaseRootPath() / std::filesystem::path(importedAssetPath.string())).string().c_str(),
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
            std::ofstream outf;
            outf.open(importDatabase->GetImportDatabaseRootPath() / importedAssetPath, std::ios::trunc | std::ios::out | std::ios::binary);
            if (outf.good() && f.is_open())
            {
                outf << f.rdbuf();
            }
        }
    }
    meta["lastImportedWriteTime"] = std::filesystem::last_write_time(absoluteAssetPath).time_since_epoch().count();
    meta["importedKtxFile"] = importedAssetPath.string();
    meta["importFileUUID"] = importFileUUID;

    option["linearFormat"] = linearFormat;
    option["generateMipmap"] = generateMipmap;
    option["convertToIrradianceCubemap"] = converToIrradianceCubemap;
    option["convertToReflectanceCubemap"] = convertToReflectanceCubemap;
    option["convertToCubemap"] = convertToCubemap;
    meta["importOption"] = option;

    return {importedAssetPath};
}
