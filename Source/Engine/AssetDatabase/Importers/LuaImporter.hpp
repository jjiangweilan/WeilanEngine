#pragma once

#include "AssetImporter.hpp"
#include "Core/LuaScript.hpp"

class LuaImporter : public AssetImporter
{
    DECLARE_ASSET_IMPORTER()

public:
    std::vector<std::filesystem::path> Import() override { return {}; }
    bool ImportNeeded() override { return false; }

    static const std::vector<std::type_index>& GetImportTypes()
    {
        static std::vector<std::type_index> types = {typeid(LuaScript)};
        return types;
    }
};
