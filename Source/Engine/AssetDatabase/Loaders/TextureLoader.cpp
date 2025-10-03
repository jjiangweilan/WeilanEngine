#include "TextureLoader.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"
#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/Image.hpp"
#include "GfxDriver/Vulkan/Internal/VKEnumMapper.hpp"
#include "Libs/Image/ImageProcessing.hpp"
#include "Libs/Utils.hpp"
#include "Rendering/Material.hpp"
#include "ThirdParty/stb/stb_image.h"
#include <fstream>
#include <ktx.h>
#include <ktxvulkan.h>

DEFINE_ASSET_LOADER(TextureLoader, "ktx2,ktx,jpg,png,jpeg,bmp,hdr,psd,tga,gif,pic,pgm,ppm")

const std::vector<std::type_index>& TextureLoader::GetImportTypes()
{
    static std::vector<std::type_index> types = {typeid(Texture)};
    return types;
}

bool TextureLoader::IsKTX2File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x32 && imageData[6] == 0x30 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[11] == 0x0A;
}

bool TextureLoader::IsKTX1File(ktx_uint8_t* imageData)
{
    return imageData[0] == 0xAB && imageData[1] == 0x4B && imageData[2] == 0x54 && imageData[3] == 0x58 &&
           imageData[4] == 0x20 && imageData[5] == 0x31 && imageData[6] == 0x31 && imageData[7] == 0xBB &&
           imageData[8] == 0x0D && imageData[9] == 0x0A && imageData[10] == 0x1A && imageData[110] == 0x0A;
}

void TextureLoader::Load()
{
    std::string importedSourcePath = meta["importedKtxFile"];

    auto sourceBinaryVec = importDatabase->ReadFile(importedSourcePath);
    uint8_t* sourceBinary = sourceBinaryVec.data();
    size_t binarySize = sourceBinaryVec.size();

    this->texture = std::make_unique<Texture>(KtxTexture{sourceBinary, binarySize});
    this->texture->SetName(absoluteAssetPath.filename().string());
}

void TextureLoader::HandleReload(Asset* loaded)
{
    Texture* tex = dynamic_cast<Texture*>(loaded);
    if (tex)
    {
        Material::RebuildAllMaterials();
    }
}
