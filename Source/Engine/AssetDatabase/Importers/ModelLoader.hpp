#pragma once
#include "AssetLoader.hpp"

class ModelLoader : public AssetLoader
{
    bool ImportNeeded() override;
    void Import() override;
    void Load() override;
    std::unique_ptr<Asset> RetrieveAsset() override
    {
        return nullptr;
    }
};
