#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Resource.hpp"
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
    uint32_t byteSize;
};
class Texture : public Resource
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
    RefPtr<Gfx::Image> GetGfxImage()
    {
        return image;
    };
    const TextureDescription& GetDescription()
    {
        return desc;
    }
    void Reload(Resource&& loaded) override;

private:
    TextureDescription desc;
    UniPtr<Gfx::Image> image;
};

UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* byteData, std::size_t byteSize);

bool IsKTX1File(ktx_uint8_t* imageData);
bool IsKTX2File(ktx_uint8_t* imageData);
} // namespace Engine
