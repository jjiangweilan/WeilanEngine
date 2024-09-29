#pragma once
#include "Asset.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <ktx.h>

struct TextureDescription
{
    Gfx::ImageDescription img;

    uint8_t* data = nullptr;
    bool keepData = false;

    // currently we can only create a 2D texture from texture description
    // the following members started with _ are filled when creating uisng ktxTexture
};

struct KtxTexture
{
    ktx_uint8_t* imageData;
    size_t byteSize;
};

enum class ImageDataType
{
    Ktx,
    StbSupported, // jpg, png, etc...
};

class Texture : public Asset
{
    DECLARE_ASSET();

public:
    Texture(){};
    // load a texture from file
    Texture(const char* path, const UUID& uuid = UUID{});
    Texture(
        uint8_t* data,
        size_t byteSize,
        ImageDataType imageDataType,
        Gfx::ImageFormat format = Gfx::ImageFormat::Invalid,
        const UUID& uuid = UUID{}
    );
    Texture(TextureDescription texDesc, const UUID& uuid = UUID{});
    Texture(KtxTexture texDesc, const UUID& uuid = UUID{});
    ~Texture() override;
    Gfx::Image* GetGfxImage()
    {
        return image.get();
    };
    const TextureDescription& GetDescription()
    {
        return desc;
    }
    void Reload(Asset&& loaded) override;

    bool IsExternalAsset() override
    {
        return true;
    }

    // return false if loading failed
    bool LoadFromFile(const char* path) override;

    void SaveAsCubemap(const char* filename);

private:
    TextureDescription desc;
    std::unique_ptr<Gfx::Image> image;
    void LoadKtxTexture(uint8_t* data, size_t byteSize);
    void LoadKtxTexture(ktxTexture2* texture, int gpuMipLevels);
    void LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize, Gfx::ImageFormat format);
    void ConvertRawImageToKtx(TextureDescription& desc);
    void CreateGfxImage(TextureDescription& texDesc);
};

bool IsKTX1File(ktx_uint8_t* imageData);
bool IsKTX2File(ktx_uint8_t* imageData);
