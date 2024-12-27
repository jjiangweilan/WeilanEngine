#pragma once

#include "AssetLoader.hpp"
class ShaderLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER()

public:
    bool ImportNeeded() override;
    std::vector<std::filesystem::path> Import() override { return {}; }
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override { return std::move(asset); }

    static const std::vector<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<Asset> asset;
};
