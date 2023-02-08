#pragma once
#include "AssetObject.hpp"
#include "GfxDriver/GfxDriver.hpp"

namespace Engine
{
using TextureDescription = Gfx::ImageDescription;

class Texture : public AssetObject
{
public:
    Texture(){};
    Texture(TextureDescription texDesc, const UUID& uuid = UUID::empty);
    ~Texture() override
    {
        if (image->GetDescription().keepData)
        {
            delete image->GetDescription().data;
        }
    }
    RefPtr<Gfx::Image> GetGfxImage() { return image; };
    const TextureDescription& GetDescription() { return image->GetDescription(); }
    void Reload(AssetObject&& loaded) override;

private:
    bool Serialize(AssetSerializer&) override { return false; } // disable saving
    UniPtr<Gfx::Image> image;
};

UniPtr<Engine::Texture> LoadTextureFromBinary(unsigned char* byteData, std::size_t byteSize);
} // namespace Engine
