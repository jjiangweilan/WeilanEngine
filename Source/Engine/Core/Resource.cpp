#include "Resource.hpp"

namespace Engine
{
std::unique_ptr<std::unordered_map<ResourceID, std::function<std::unique_ptr<Resource>()>>>
    ResourceRegister::resourceRegisters = nullptr;

char ResourceRegister::RegisterResource(const ResourceID& id, const std::function<std::unique_ptr<Resource>()>& f)
{
    GetResourceRegisters()->emplace(id, f);
    return '0';
}

std::unique_ptr<Resource> ResourceRegister::CreateResource(const ResourceID& id)
{
    return GetResourceRegisters()->at(id)();
};

const std::unique_ptr<std::unordered_map<ResourceID, std::function<std::unique_ptr<Resource>()>>>& ResourceRegister::
    GetResourceRegisters()
{
    if (resourceRegisters == nullptr)
    {
        resourceRegisters =
            std::make_unique<std::unordered_map<ResourceID, std::function<std::unique_ptr<Resource>()>>>();
    }

    return resourceRegisters;
}
} // namespace Engine
