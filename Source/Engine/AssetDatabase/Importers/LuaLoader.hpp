#pragma once

#include "AssetLoader.hpp"

class LuaLoader : public AssetLoader
{
    DECLARE_ASSET_LOADER()

public:
    bool ImportNeeded() override
    {
        return false;
    }
    DynamicArray<std::filesystem::path> Import() override {return {};}

    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return std::move(asset);
    }

    static const DynamicArray<std::type_index>& GetImportTypes();

private:
    std::unique_ptr<Asset> asset;
};
