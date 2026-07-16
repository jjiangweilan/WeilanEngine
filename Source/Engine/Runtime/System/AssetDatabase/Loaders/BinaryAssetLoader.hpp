#pragma once

#include "AssetLoader.hpp"
#include "Engine/Core/BinaryAsset.hpp"

class BinaryAssetLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER();

public:
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override;

    static const std::vector<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<BinaryAsset> asset;
};
