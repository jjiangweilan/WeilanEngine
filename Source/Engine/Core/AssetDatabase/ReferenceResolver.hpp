#pragma once

#include "Code/UUID.hpp"
#include "Code/Ptr.hpp"
#include <functional>
#include <vector>
namespace Engine
{
    class AssetDatabase;
    class AssetObject;
    class IReferenceResolveListener
    {
        public:
            virtual void OnReferenceResolve(void* ptr, AssetObject* resolved) = 0;
        private:
    };

    class ReferenceResolver
    {
        public:
            void ResolveReference(const UUID& uuid, AssetObject*& ptr, IReferenceResolveListener* resolveListener = nullptr);
            //template<class T>
            //void ResolveReference(const UUID& uuid, RefPtr<T>& ptr, IReferenceResolveListener* resolveListener = nullptr)
            //{
            //    Pending p {PtrType::Raw, uuid, (void*)&ptr.Get(), resolveListener};
            //    pending.push_back(p);
            //}

            /**
             * Used by AssetDatabase
             */
            void ResolveAllPending(AssetDatabase& assetDatabase);

        private:
            enum class PtrType
            {
                Raw,
                //RefPtr
            };

            struct Pending
            {
                PtrType type;
                UUID uuid;
                void* val;
                IReferenceResolveListener* refListener = nullptr;
            };

            std::vector<Pending> pending;
            std::vector<Pending> remain;

    };
}
