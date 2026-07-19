#include "TerrainConfigAssetUtility.hpp"

#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include <memory>

namespace Editor
{
TerrainConfig* CreateTerrainConfigAsset(AssetDatabase& assetDatabase, const AssetPath& path)
{
    Asset* savedAsset = assetDatabase.SaveAsset(std::make_unique<TerrainConfig>(), path);
    TerrainConfig* terrainConfig = dynamic_cast<TerrainConfig*>(savedAsset);
    if (terrainConfig == nullptr)
        return nullptr;

    const AssetPath& configPath = assetDatabase.GetAssetPath(terrainConfig->GetUUID());
    const AssetPath heightPath = configPath.GetParentPath() /
                                 AssetPath(configPath.GetFileNameWithoutExtension() + " Heightmap");
    Asset* savedHeightAsset = assetDatabase.SaveAsset(std::make_unique<BinaryAsset>(), heightPath);
    BinaryAsset* heightAsset = dynamic_cast<BinaryAsset*>(savedHeightAsset);
    if (heightAsset == nullptr)
        return terrainConfig;

    terrainConfig->SetHeightDataAsset(heightAsset);
    terrainConfig->InitializeFlatHeightMap();
    assetDatabase.SaveAsset(*heightAsset);
    assetDatabase.SaveAsset(*terrainConfig);
    return terrainConfig;
}
} // namespace Editor
