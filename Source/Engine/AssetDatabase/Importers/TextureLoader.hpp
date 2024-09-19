#include "AssetLoader.hpp"

// converting image files to ktx file
class TextureLoader : public AssetLoader
{
public:
    bool ImportNeeded() override;
    void Import() override;
    void Load() override;
    void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap) override;
    std::unique_ptr<Asset> RetrieveAsset() override;

private:
    void LoadStbSupoprtedTexture(uint8_t* data, size_t byteSize);
};
