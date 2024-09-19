#include "AssetLoader.hpp"
#include "ktx.h"

// converting image files to ktx file
class TextureLoader : public AssetLoader
{
public:
    bool ImportNeeded() override;
    void Import() override;
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return std::move(texture);
    }

private:
    void LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize);
    std::unique_ptr<Asset> texture;
    bool IsKTX2File(ktx_uint8_t* imageData);
    bool IsKTX1File(ktx_uint8_t* imageData);
};
