#pragma once

#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"

class AssetDatabase;
class TerrainConfig;

namespace Editor
{
TerrainConfig* CreateTerrainConfigAsset(AssetDatabase& assetDatabase, const AssetPath& path);
} // namespace Editor
