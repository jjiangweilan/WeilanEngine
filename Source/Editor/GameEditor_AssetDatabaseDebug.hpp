#pragma once

#include "Engine/Core/Object.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include <string>
#include <unordered_map>
#include <vector>

class AssetData;

namespace Editor
{
class GameEditorAssetDatabaseDebug
{
public:
    void Show(bool& open);

    struct EngineObjectInfo
    {
        UUID uuid;
        ObjPtr<Object> object;
        std::string typeName;
    };

    struct AssetDataInfo
    {
        AssetData* data;
        UUID assetUUID;
        AssetPath path;
        std::string uuidText;
        bool loaded;
        bool fileExists;
    };

    struct DuplicateUUIDEntry
    {
        std::string kind;
        std::string path;
        std::string label;
    };

    using DuplicateUUIDMap = std::unordered_map<std::string, std::vector<DuplicateUUIDEntry>>;

private:
    void RefreshCache();
    void DrawSummary();
    void DrawDuplicateUUIDReport();
    void DrawAssetDataTable();
    void DrawEngineObjectTable();

    DuplicateUUIDMap duplicateUUIDs;
    bool duplicateScanRan = false;
    bool cacheBuilt = false;
    Object::EngineObjectMap loadedObjects;
    std::vector<EngineObjectInfo> engineObjects;
    std::vector<AssetDataInfo> assetDatas;
};
} // namespace Editor
