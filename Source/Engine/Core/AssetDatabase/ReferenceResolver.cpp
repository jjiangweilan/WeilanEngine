#include "ReferenceResolver.hpp"
#include "AssetDatabase.hpp"
namespace Engine
{
void ReferenceResolver::ResolveReference(const UUID& uuid,
                                         AssetObject*& ptr,
                                         IReferenceResolveListener* resolveListener)
{
    Pending p{PtrType::Raw, uuid, (void*)&ptr, resolveListener};
    pending.push_back(p);
}

void ReferenceResolver::ResolveAllPending(AssetDatabase& assetDatabase)
{
    for (auto& p : pending)
    {
        auto assetObj = assetDatabase.GetAssetObject(p.uuid);
        if (assetObj)
        {
            if (p.type == PtrType::Raw)
            {
                *((AssetObject**)p.val) = assetObj.Get();
            }
            /*            else if (p.type == PtrType::RefPtr)
                          {
                              *(RefPtr<AssetObject>*)p.val = assetObj.Get();
                          }*/

            if (p.refListener != nullptr)
            {
                p.refListener->OnReferenceResolve(p.val, assetObj.Get());
            }

            resolved[p.uuid].push_back(p);
        }
        else
        {
            remain.push_back(p);
        }
    }

    pending.clear();
    std::swap(pending, remain);
}

void ReferenceResolver::Reresolve(UUID uuid)
{
    auto iter = resolved.find(uuid);
    if (iter != resolved.end())
    {
        auto assetObj = AssetDatabase::Instance()->GetAssetObject(uuid);
        for (auto& p : iter->second)
        {
            if (assetObj)
            {
                *((AssetObject**)p.val) = assetObj.Get();
            }

            if (p.refListener != nullptr)
            {
                p.refListener->OnReferenceResolve(p.val, assetObj.Get());
            }
        }
    }
}
} // namespace Engine
