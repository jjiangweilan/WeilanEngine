#include "AssetLoader.hpp"
#include "Libs/Serialization/JsonSerializer.hpp"

// converting image files to ktx file
class InternalAssetLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER();

public:
    bool ImportNeeded() override { return false; };
    DynamicArray<std::filesystem::path> Import() override { return {}; }
    bool IsInternalAsset() override { return true; }

    void Load() override;
    void GetReferenceResolveData(Serializer*& serializer, SerializeReferenceResolveMap*& resolveMap) override;
    std::unique_ptr<Asset> RetrieveAsset() override { return std::move(asset); }

    static const DynamicArray<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<Asset> asset;
    JsonSerializer ser;
    SerializeReferenceResolveMap resolveMap;
};
