#include "MaterialUploadManager.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include <vector>

MaterialUploadManager& MaterialUploadManager::Instance()
{
    static MaterialUploadManager instance;
    return instance;
}

void MaterialUploadManager::AddPendingUpload(Material* material)
{
    if (material == nullptr)
        return;

    ScopedSpinLock scoped(lock);
    pendingUploads.insert(material);
}

void MaterialUploadManager::RemovePendingUpload(Material* material)
{
    if (material == nullptr)
        return;

    ScopedSpinLock scoped(lock);
    pendingUploads.erase(material);
}

void MaterialUploadManager::FlushPendingUploads()
{
    std::vector<Material*> uploads;
    {
        ScopedSpinLock scoped(lock);
        if (pendingUploads.empty())
            return;

        uploads.assign(pendingUploads.begin(), pendingUploads.end());
        pendingUploads.clear();
    }

    for (auto* material : uploads)
    {
        if (material != nullptr && material->HasPendingGPUMaterialUpload() && material->IsGPUMaterialRegistered())
            material->UpdateGPUMaterialData();
    }
}
