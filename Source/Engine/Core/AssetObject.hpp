#pragma once
#include "Code/UUID.hpp"
#include "Code/Ptr.hpp"
#include "Object.hpp"
#include "AssetDatabase/AssetSerializer.hpp"
#include "AssetDatabase/ReferenceResolver.hpp"

#include <string>
#include <cassert>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <cinttypes>

namespace Engine
{
    // from https://stackoverflow.com/questions/13842468/comma-in-c-c-macro
    template<typename T> struct ArgumentType;
    template<typename T, typename U> struct ArgumentType<T(U)> { typedef U ElementType; };
    #define SINGLE_ARG(...) __VA_ARGS__

#define EDITABLE(type, name) \
            type name; \
            Editable<ArgumentType<void(type)>::ElementType> _Editable_##name = Editable<ArgumentType<void(type)>::ElementType>(this, #name, name);

    class AssetDatabase;
    class AssetFileData;
    class AssetObject : public Object, IReferenceResolveListener
    {
        DECLARE_ENGINE_OBJECT();
        public:
            /**
             * @brief Construct a new Asset Object object
             * 
             * @param ext extension of the AssetObject when saved to disk, e.g. .aom, .world
             * @param uuid 
             */
            AssetObject(const UUID& uuid = UUID::empty);
            virtual ~AssetObject() {};

            const UUID& GetUUID() const;

            // return bool to indicate if this Asset needs to be serialized
            virtual bool Serialize(AssetSerializer& serializer);
            virtual void Deserialize(AssetSerializer& serializer, ReferenceResolver& refResolver);

            template<class T>
            T* GetMember(const std::string& name)
            {
                auto iter = editableMembers.find(name);
                if (iter != editableMembers.end())
                {
                    if (iter->second.typeInfo == typeid(T))
                    {
                        return (T*)iter->second.editableVal;
                    }
                }

                return nullptr;
            }

            std::vector<RefPtr<AssetObject>> GetAssetObjectMembers();

        protected:
            class EditableBase
            {
            public:
                EditableBase(
                    AssetObject* parent,
                    const std::string& name) :  name(name), parent(parent)
                {}
                
                const std::string& GetName() {return name;}
                AssetObject& GetParent() {return *parent;}
                // provides a way to safe casting val (with it't void pointer) to AssetObject*
                virtual void Serialize(const std::string& nameChain, AssetSerializer& serializer) = 0;
                virtual void Deserialize(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver) = 0;

            protected:
                std::string name;
                AssetObject* parent;
            };

            template<class T>
            class Editable : public EditableBase
            {
            public:
                template<class... Args>
                Editable(AssetObject* parent, const std::string& name, T& val) : 
                    EditableBase(parent, name),
                    val(val)
                {
                    std::vector<RefPtr<AssetObject>> assetMembers;
                    ExtractContainedAssetObjects(val, assetMembers);
                    parent->assetMembers.insert(parent->assetMembers.end(), assetMembers.begin(), assetMembers.end());
                    parent->editableMembers.emplace(
                        std::make_pair<std::string, EditableMember>(
                        std::string(name),
                        EditableMember(
                            typeid(T),
                            this,
                            (void*)&val)
                        )
                    );
                }

                void Serialize(const std::string& nameChain, AssetSerializer& serializer) override
                {
                    SerializePrivate(nameChain, serializer, val);
                }

                void Deserialize(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver) override
                {
                    DeserializePrivate(nameChain, serializer, refResolver, val);
                }

                inline T& operator*() const {return val;}
                inline operator T&() {return val;}
                inline T& GetVal() {return val;}

            private:
                T& val;

                template<class U>
                static void ExtractContainedAssetObjects(U& val, std::vector<RefPtr<AssetObject>>& output)
                {
                    if constexpr (IsVector<U>::value)
                    {
                        for(auto& sub : val)
                        {
                            ExtractContainedAssetObjects(sub, output);
                        }
                    }
                    else if constexpr (IsUniPtr<U>::value || IsRefPtr<U>::value || std::is_pointer<U>::value)
                    {
                        ExtractContainedAssetObjects(*val, output);
                    }
                    else if constexpr (IsUnorderedMap<U>::value)
                    {
                        for (auto& iter : val)
                        {
                            ExtractContainedAssetObjects(iter.second, output);
                        }
                    }
                    else if constexpr (std::is_base_of_v<AssetObject, U>)
                    {
                        output.push_back(&val);
                    }
                }

                template<class U> struct DecayPtr { using Type = U; };
                template<class U> struct DecayPtr<UniPtr<U>> { using Type = U; };
                template<class U> struct DecayPtr<RefPtr<U>> { using Type = U; };
                template<class U> struct DecayPtr<U*> { using Type = U; };

                template<class U> struct IsRefPtr : std::false_type {};
                template<class U> struct IsRefPtr<RefPtr<U>> : std::true_type {};

                template<class U> struct IsUniPtr : std::false_type {};
                template<class U> struct IsUniPtr<UniPtr<U>> : std::true_type {};

                template<class U> struct IsVector : std::false_type {};
                template<class U> struct IsVector<std::vector<U>> : std::true_type {};

                template<class U> struct IsUnorderedMap : std::false_type {};
                template<class _Key, class _Value> struct IsUnorderedMap<std::unordered_map<_Key, _Value>> : std::true_type {};

                template<class U>
                void SerializePrivate(const std::string& nameChain, AssetSerializer& serializer, U& val)
                {
                    // case: where T is an RefPtr
                    if constexpr(IsRefPtr<U>::value)
                    {
                        if (val != nullptr)
                            // we have to do reinterpret_cast here for cases where U is forward declared. The policy of Editable enfores a RefPtr or a raw pointer
                            // is pointing to an AssetObject
                            serializer.Write(nameChain, reinterpret_cast<AssetObject*>(val.Get())->GetUUID());
                        else
                            serializer.Write(nameChain, UUID::empty);
                    }
                    // case; where T is an raw ptr
                    else if constexpr (std::is_pointer<U>::value)
                    {
                        if (val != nullptr)
                            serializer.Write(nameChain, reinterpret_cast<AssetObject*>(val)->GetUUID());
                        else
                            serializer.Write(nameChain, UUID::empty);
                    }
                    // case: where T is an AssetObject and not any pointer
                    else if constexpr (
                        std::is_base_of_v<AssetObject, typename DecayPtr<U>::Type> &&
                        std::is_same_v<U, typename DecayPtr<U>::Type>)
                    {
                        val.SerializeSelf(nameChain, serializer);
                        val.SerializePrivate(nameChain, serializer);
                    }
                    // case : where T is an UniPtr
                    else if constexpr(IsUniPtr<U>::value)
                    {
                        if (val != nullptr)
                        {
                            serializer.Write<char>(nameChain, 1);
                            SerializePrivate<typename DecayPtr<U>::Type>(nameChain, serializer, *val);
                        }
                        else
                        {
                            // val is a nullptr, indicate it in serializer
                            serializer.Write<char>(nameChain, 0);
                        }
                    }
                    else if constexpr(IsVector<U>::value)
                    {
                        uint32_t i = 0;
                        for(auto& v : val)
                        {
                            std::ostringstream oss;
                            oss << nameChain << "." << i;
                            SerializePrivate(oss.str(), serializer, v);
                            
                            i += 1;
                        }

                        std::ostringstream oss;
                        oss << nameChain << ".size";
                        serializer.Write(oss.str(), (uint32_t)val.size());
                    }
                    else if constexpr(IsUnorderedMap<U>::value)
                    {
                        std::vector<typename U::key_type> keys;

                        for(auto& iter : val)
                        {
                            std::ostringstream oss;
                            oss << nameChain << "." << iter.first;
                            SerializePrivate(oss.str(), serializer, iter.second);

                            keys.push_back(iter.first);
                        }

                        std::ostringstream oss;
                        oss << nameChain << "._keys_";
                        // we key use the serializer.Write here, but at the time of writing this. Write's Vector serialization support is not written yet
                        SerializePrivate(oss.str(), serializer, keys);
                    }
                    else
                    {
                        serializer.Write(nameChain, val);
                    }
                }

                template<class U>
                void DeserializePrivate(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver, U& val)
                {
                    // case: where T is a raw pointer
                    if constexpr(std::is_pointer<U>::value)
                    {
                        UUID id = UUID::empty;
                        serializer.Read(nameChain,id);
                        if (id != UUID::empty)
                        {
                            refResolver.ResolveReference(id, reinterpret_cast<AssetObject*&>(val), parent);
                        }
                        else
                            val = nullptr;
                    }
                    // case: where T is an RefPtr
                    else if constexpr (IsRefPtr<U>::value)
                    {
                        UUID id = UUID::empty;
                        serializer.Read(nameChain, id);
                        if (id != UUID::empty)
                        {
                            refResolver.ResolveReference(id, reinterpret_cast<AssetObject*&>(val.GetPtrRef()), parent);
                        }
                        else
                            val = nullptr;
                    }
                    // case: where T is an AssetObject and not any pointer
                    else if constexpr (
                        std::is_base_of_v<AssetObject, typename DecayPtr<U>::Type> &&
                        std::is_same_v<U, typename DecayPtr<U>::Type>)
                    {
                        // cast allows DeserializePrivate to be a private method in derived class
                        static_cast<AssetObject*>(&val)->DeserializeInternal(nameChain, serializer, refResolver);
                    }
                    // case : where T is an UniPtr
                    else if constexpr(IsUniPtr<U>::value)
                    {
                        char nullTest = 0;
                        serializer.Read(nameChain, nullTest);
                        // nullTest can only be 0 or 1 see the SerializePrivate counterpart
                        assert(nullTest == 0 || nullTest == 1);
                        using ActualType = typename DecayPtr<U>::Type;
                        if (nullTest == 1)
                        {
                            if constexpr (!std::is_abstract_v<ActualType>)
                            {
                                val = MakeUnique<ActualType>();
                            }
                            else
                            {
                                std::ostringstream nameMemberNameChain;
                                nameMemberNameChain << nameChain << ".className";
                                std::string className;
                                serializer.Read(nameMemberNameChain.str(), className);
                                val = ActualType::CreateDerived(className);
                             }

                            if constexpr (std::is_base_of_v<AssetObject, ActualType>)
                            {
                                static_cast<AssetObject*>(val.Get())->DeserializeInternal(nameChain, serializer, refResolver);
                            }
                            else
                            {
                                DeserializePrivate(nameChain, serializer, refResolver, *val);
                            }
                        }
                        else
                        {
                            val = nullptr;
                        }
                    }
                    else if constexpr(IsVector<U>::value)
                    {
                        std::ostringstream oss;
                        oss << nameChain << ".size";
                        uint32_t size = 0;
                        serializer.Read(oss.str(), size);

                        if (size != 0)
                        {
                            val.clear();
                            val.reserve(size);
                            for(uint32_t i = 0; i < size; ++i)
                            {
                                std::ostringstream oss;
                                oss << nameChain << "." << i;

                                val.emplace_back();
                                DeserializePrivate(oss.str(), serializer, refResolver, val.back());
                            }
                        }
                    }
                    else if constexpr(IsUnorderedMap<U>::value)
                    {
                        std::ostringstream oss;
                        oss << nameChain << "._keys_";
                        std::vector<typename U::key_type> keys;
                        DeserializePrivate(oss.str(), serializer, refResolver, keys);
                        for(auto& key : keys)
                        {
                            std::ostringstream oss;
                            oss << nameChain << "." << key;
                            typename U::mapped_type element;
                            val[key] = typename U::mapped_type();
                            DeserializePrivate(oss.str(), serializer, refResolver, val[key]);
                        }
                    }
                    else
                    {
                        serializer.Read(nameChain, val);
                    }
                }
            };

            void OnReferenceResolve(void* ptr, AssetObject* resolved) override {}

        protected:

            // this function gives subclass a chance to handle member dependencies when the object is inside a deserialization chain
            virtual void DeserializeInternal(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver);
            virtual bool SerializeInternal(const std::string& nameChain, AssetSerializer& serializer);

        private:

            UUID uuid;

            struct EditableMember
            {
                EditableMember(
                        const std::type_info& typeInfo,
                        EditableBase* editable,
                        void* editableVal) : typeInfo(typeInfo), editable(editable), editableVal(editableVal) {};
                const std::type_info& typeInfo;
                EditableBase* editable;
                void* editableVal;
            };
            std::unordered_map<std::string, EditableMember> editableMembers;
            std::vector<RefPtr<AssetObject>> assetMembers;

            void SerializePrivate(const std::string& nameChain, AssetSerializer& serializer);
            void SerializeSelf(const std::string& nameChain, AssetSerializer& serializer);
            void DeserializePrivate(const std::string& nameChain, AssetSerializer& serializer, ReferenceResolver& refResolver);
            void DeserializeSelf(const std::string& nameChain, AssetSerializer& serializer);
    };
}
