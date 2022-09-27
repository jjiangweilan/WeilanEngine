#pragma once

#include "Code/UUID.hpp"
#include "Code/Ptr.hpp"
#include <functional>
#include <vector>
namespace Engine
{
    class AssetDatabase;
    class AssetObject;
    class ReferenceResolver
    {
        public:
            void ResolveReference(const UUID& uuid, AssetObject*& ptr);
            template<class T>
            void ResolveReference(const UUID& uuid, RefPtr<T>& ptr)
            {
                Pending p {PtrType::RefPtr, uuid, (void*)&ptr};
                pending.push_back(p);
            }

            /**
             * Used by AssetDatabase
             */
            void ResolveAllPending(AssetDatabase& assetDatabase);

        private:
            enum class PtrType
            {
                Raw,
                RefPtr
            };

            struct Pending
            {
                PtrType type;
                UUID uuid;
                void* val;
            };

            std::vector<Pending> pending;
            std::vector<Pending> remain;

    };
}
