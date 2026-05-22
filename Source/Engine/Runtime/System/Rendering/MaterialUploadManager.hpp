#pragma once

#include "Engine/Library/SpinLock.hpp"
#include <unordered_set>

class Material;

class MaterialUploadManager
{
public:
    static MaterialUploadManager& Instance();

    void AddPendingUpload(Material* material);
    void RemovePendingUpload(Material* material);
    void FlushPendingUploads();

private:
    Spinlock lock;
    std::unordered_set<Material*> pendingUploads;
};
