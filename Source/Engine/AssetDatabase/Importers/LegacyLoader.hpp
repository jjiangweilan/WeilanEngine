#include "AssetLoader.hpp"

class LegacyLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER()

public:
    bool ImportNeeded() override
    {
        return false;
    }
    void Import() override {}
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return std::move(asset);
    }

    static const std::vector<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<Asset> asset;
};
