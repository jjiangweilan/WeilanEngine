#include "AssetLoader.hpp"
#include "ktx.h"

// import options
/**
 * importOption : {
 *     generateMipmap : bool
 * }
 */
class TextureLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER()

public:
    bool ImportNeeded() override;
    void Import() override;
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return std::move(texture);
    }
    static const std::vector<std::type_index>& GetImportTypes();

private:
    void LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize);
    std::unique_ptr<Asset> texture;
    bool IsKTX2File(ktx_uint8_t* imageData);
    bool IsKTX1File(ktx_uint8_t* imageData);
};
