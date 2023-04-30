#include "ReferenceResolver.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"

namespace Engine
{
void ReferenceResolver2::Resolve(AssetDatabase& assetDatabase)
{
    for (auto& v : pointers)
    {
        auto asset = assetDatabase.GetAssetObject(v.uuid);
        if (asset != nullptr)
            v.ptr = &*asset;
    }

    pointers.clear();
}
} // namespace Engine
