#pragma once
#include "Asset.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <ktx.h>

namespace Engine
{
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
class Texture : public Asset
{
    DECLARE_ASSET();

public:
    Texture(){};
    // load a ktx texture from file
    Texture(const char* path, const UUID& uuid = UUID::empty);
    Texture(TextureDescription texDesc, const UUID& uuid = UUID::empty);
    Texture(KtxTexture texDesc, const UUID& uuid = UUID::empty);
    ~Texture() override
    {
        if (desc.keepData && desc.data != nullptr)
        {
            delete desc.data;
        }
    }
    RefPtr<Gfx::Image> GetGfxImage()
    {
        return image;
    };
    const TextureDescription& GetDescription()
    {
        return desc;
    }
    void Reload(Asset&& loaded) override;

private:
    TextureDescription desc;
    UniPtr<Gfx::Image> image;
    void LoadKtxTexture(uint8_t* data, size_t byteSize);
};

UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* byteData, std::size_t byteSize);

bool IsKTX1File(ktx_uint8_t* imageData);
bool IsKTX2File(ktx_uint8_t* imageData);
} // namespace Engine
