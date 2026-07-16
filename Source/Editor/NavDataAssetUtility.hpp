#pragma once

#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"

class AssetDatabase;
class BinaryAsset;
class NavData;

namespace Editor
{
NavData* CreateNavDataAsset(AssetDatabase& assetDatabase, const AssetPath& path);
BinaryAsset* EnsureNavDataCellAsset(AssetDatabase& assetDatabase, NavData& navData);
} // namespace Editor
