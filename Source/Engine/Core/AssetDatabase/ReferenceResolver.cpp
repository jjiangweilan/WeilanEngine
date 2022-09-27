#include "ReferenceResolver.hpp"
#include "AssetDatabase.hpp"
namespace Engine
{
    void ReferenceResolver::ResolveReference(const UUID& uuid, AssetObject*& ptr)
    {
        Pending p {PtrType::Raw, uuid, (void*)&ptr};
        pending.push_back(p);
    }

    void ReferenceResolver::ResolveAllPending(AssetDatabase& assetDatabase)
    {
        for(auto& p : pending)
        {
            auto assetObj = assetDatabase.GetAssetObject(p.uuid);
            if (assetObj)
            {
                if (p.type == PtrType::Raw)
                {
                    *(AssetObject**)p.val = assetObj.Get();
                }
                else if (p.type == PtrType::RefPtr)
                {
                    *(RefPtr<AssetObject>*)p.val = assetObj.Get();
                }
            }
            else
            {
                remain.push_back(p);
            }
        }

        pending.clear();
        std::swap(pending, remain);
    }
}