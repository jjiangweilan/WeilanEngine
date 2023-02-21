#pragma once
#include "AssetObject.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include <ktx.h>

namespace Engine
{
struct TextureDescription
{
    Gfx::ImageDescription img;

    uint8_t* data = nullptr; // this pointer needs to be managed
    bool keepData = false;   // not used by GfxDriver, it's provided for abstraction layer
    bool isCubemap = false;
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
