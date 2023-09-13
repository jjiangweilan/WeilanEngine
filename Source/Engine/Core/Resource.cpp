#include "Resource.hpp"
namespace Engine
{
std::unordered_map<ResourceTypeID, std::function<std::unique_ptr<Resource>()>>* ResourceRegistry::GetRegisteredAsset()
{
    static std::unique_ptr<std::unordered_map<ResourceTypeID, ResourceRegistry::Creator>> registeredAsset =
        std::make_unique<std::unordered_map<ResourceTypeID, ResourceRegistry::Creator>>();
    return registeredAsset.get();
}

char ResourceRegistry::RegisterAsset(const ResourceTypeID& assetID, const Creator& creator)
{
    GetRegisteredAsset()->emplace(assetID, creator);
    return '0';
}

std::unique_ptr<Resource> ResourceRegistry::CreateAsset(const ResourceTypeID& id)
{
    auto iter = GetRegisteredAsset()->find(id);
    if (iter != GetRegisteredAsset()->end())
    {
        return iter->second();
    }

    return nullptr;
}

} // namespace Engine
