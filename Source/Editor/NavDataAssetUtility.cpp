#include "NavDataAssetUtility.hpp"

#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/Navigation/NavData.hpp"
#include <memory>

namespace Editor
{
BinaryAsset* EnsureNavDataCellAsset(AssetDatabase& assetDatabase, NavData& navData)
{
    if (BinaryAsset* existing = navData.GetCellAsset())
        return existing;

    const AssetPath& navPath = assetDatabase.GetAssetPath(navData.GetUUID());
    if (navPath.empty())
        return nullptr;

    const AssetPath cellPath =
        navPath.GetParentPath() / AssetPath(navPath.GetFileNameWithoutExtension() + " Cells");
    Asset* savedAsset = assetDatabase.SaveAsset(std::make_unique<BinaryAsset>(), cellPath);
    BinaryAsset* binaryAsset = dynamic_cast<BinaryAsset*>(savedAsset);
    if (binaryAsset == nullptr)
        return nullptr;

    navData.SetCellAsset(binaryAsset);
    navData.WriteCellsToBinary();
    assetDatabase.SaveAsset(*binaryAsset);
    assetDatabase.SaveAsset(navData);
    return binaryAsset;
}

NavData* CreateNavDataAsset(AssetDatabase& assetDatabase, const AssetPath& path)
{
    Asset* savedAsset = assetDatabase.SaveAsset(std::make_unique<NavData>(), path);
    NavData* navData = dynamic_cast<NavData*>(savedAsset);
    if (navData == nullptr)
        return nullptr;

    EnsureNavDataCellAsset(assetDatabase, *navData);
    return navData;
}
} // namespace Editor
