#pragma once
#include "AssetObject.hpp"
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
    bool _isCubemap = false;
};

struct KtxTexture
{
    ktx_uint8_t* imageData;
    uint32_t byteSize;
};
class Texture : public AssetObject
{
public:
    Texture(){};
    Texture(TextureDescription texDesc, const UUID& uuid = UUID::empty);
    Texture(KtxTexture texDesc, const UUID& uuid = UUID::empty);
    ~Texture() override
    {
        if (desc.keepData && desc.data != nullptr)
        {
            delete desc.data;
        }
    }
    RefPtr<Gfx::Image> GetGfxImage() { return image; };
    const TextureDescription& GetDescription() { return desc; }
    void Reload(AssetObject&& loaded) override;

    static UniPtr<Engine::Texture> LoadFromKtx(ktx_uint8_t* imageData, uint32_t byteSize);

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable saving
    TextureDescription desc;
    UniPtr<Gfx::Image> image;
};

UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* byteData, std::size_t byteSize);
} // namespace Engine
