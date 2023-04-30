#pragma once

#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
namespace Engine
{
class AssetObject;
class AssetDatabase;
class ReferenceResolver2
{
public:
    void ResolvePointer(void*& ptr, const UUID& uuid) { pointers.emplace_back(ptr, uuid); }
    void Resolve(AssetDatabase& assetDatabase);

private:
    struct PointerToResolve
    {
        PointerToResolve(void*& ptr, const UUID& uuid) : ptr(ptr), uuid(uuid) {}
        void*& ptr;
        UUID uuid;
    };

    std::vector<PointerToResolve> pointers;
};
} // namespace Engine
