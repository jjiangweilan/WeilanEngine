#include "AssetLoader.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"

// converting image files to ktx file
class InternalAssetLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER();

public:
    bool ImportNeeded() override
    {
        return false;
    };
    void Import() override {}

    void Load() override;
    void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap) override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return std::move(asset);
    }

    static const std::vector<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<Asset> asset;
    JsonSerializer ser;
    SerializeReferenceResolveMap resolveMap;
};
