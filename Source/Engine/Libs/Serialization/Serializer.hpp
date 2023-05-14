#pragma once
#include "HasUUIDConcept.hpp"
#include "Libs/Internal/uuids/uuid.h"
#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include <concepts>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
namespace Engine
{

using ReferenceResolveCallback = std::function<void(void* resource)>;

class Serializer;

template <class T>
concept IsSerializable = std::derived_from<T, Serializable>;

template <class T>
struct HasUUIDContained : std::false_type
{};

template <template <class...> class T, HasUUID U>
struct HasUUIDContained<T<U>> : std::true_type
{};

class Serializer
{
public:
    Serializer(std::vector<unsigned char>&& bytes) : bytes(std::move(bytes)) {}
    Serializer() : bytes() {}
    struct ReferenceResolve
    {
        ReferenceResolve(void*& target, const UUID& targetUUID, const ReferenceResolveCallback& callback)
            : target(target), targetUUID(targetUUID), callback(callback){};
        void*& target;
        UUID targetUUID;
        ReferenceResolveCallback callback;
    };

public:
    template <class T>
    void Serialize(const T& val);
    template <class T>
    void Deserialize(T& val);

    template <>
    void Serialize(const std::string& val);
    template <>
    void Deserialize(std::string& val);

    template <>
    void Serialize(const UUID& uuid);
    template <>
    void Deserialize(UUID& uuid);

    template <class T>
    void Serialize(const std::vector<T>& val);
    template <class T>
    void Deserialize(std::vector<T>& val, const ReferenceResolveCallback& callback = nullptr);

    template <class T, class U>
    void Serialize(const std::unordered_map<T, U>& val);
    template <class T, class U>
    void Deserialize(std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback = nullptr);

    template <class T>
    void Serialize(const UniPtr<T>& val);
    template <class T>
    void Deserialize(UniPtr<T>& val);

    template <IsSerializable T>
    void Serialize(T& val);
    template <IsSerializable T>
    void Deserialize(T& val);

    template <HasUUID T>
    void Serialize(T* val);
    template <HasUUID T>
    void Deserialize(T*& val, const ReferenceResolveCallback& callback = nullptr);

    template <HasUUID T>
    void Serialize(RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(RefPtr<T>& val, const ReferenceResolveCallback& callback);

    unsigned char* GetBinary(size_t& size)
    {
        size = bytes.size();
        return &bytes[0];
    }
    inline const std::unordered_map<UUID, std::vector<ReferenceResolve>>& GetResolveCallbacks()
    {
        return resolveCallbacks;
    }

private:
    std::vector<unsigned char> bytes;
    size_t readOffset = 0;
    std::unordered_map<UUID, std::vector<ReferenceResolve>> resolveCallbacks;
};

template <class T>
concept HasReferenceResolveParamter =
    requires(Serializer s, T v, Serializer::ReferenceResolve r) { s.Deserialize(v, r); };

template <class T>
void Serializer::Serialize(const T& val)
{
    size_t size = sizeof(T);
    size_t offset = bytes.size();
    bytes.resize(offset + size);
    memcpy(&bytes[offset], &val, size);
}

template <class T>
void Serializer::Deserialize(T& val)
{
    size_t size = sizeof(val);
    memcpy(&val, bytes.data() + readOffset, size);
    readOffset += size;
}

template <class T>
void Serializer::Serialize(const std::vector<T>& val)
{
    uint32_t size = val.size();
    Serialize(size);
    for (int i = 0; i < size; ++i)
    {
        Serialize(val[i]);
    }
}

template <class T>
void Serializer::Deserialize(std::vector<T>& val, const ReferenceResolveCallback& callback)
{
    uint32_t size;
    Deserialize(size);
    val.resize(size);
    for (int i = 0; i < size; ++i)
    {
        if constexpr (HasReferenceResolveParamter<T>)
            Deserialize(val[i], callback);
        else
            Deserialize(val[i]);
    }
}

template <class T, class U>
void Serializer::Serialize(const std::unordered_map<T, U>& val)
{
    uint32_t size = val.size();
    Serialize(size);
    for (auto& iter : val)
    {
        Serialize(iter.first);
        Serialize(iter.second);
    }
}
template <class T, class U>
void Serializer::Deserialize(std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback)
{
    uint32_t size;
    Deserialize(size);
    for (int i = 0; i < size; ++i)
    {
        T key;
        U value;
        Deserialize(key);
        if constexpr (HasReferenceResolveParamter<U>)
            Deserialize(val[key], callback);
        else
            Deserialize(val[key]);
    }
}

template <class T>
void Serializer::Serialize(const UniPtr<T>& val)
{
    Serialize(*val);
}

template <class T>
void Serializer::Deserialize(UniPtr<T>& val)
{
    Deserialize(*val);
}

template <HasUUID T>
void Serializer::Serialize(T* val)
{
    if (val)
        Serialize(val->GetUUID());
    else
        Serialize(UUID::empty);
}

template <HasUUID T>
void Serializer::Deserialize(T*& val, const ReferenceResolveCallback& callback)
{
    UUID uuid;
    Deserialize(uuid);
    val = nullptr;
    if (uuid != UUID::empty)
    {
        resolveCallbacks[uuid].emplace_back((void*&)val, uuid, callback);
    }
}

template <HasUUID T>
void Serializer::Serialize(RefPtr<T>& val)
{
    Serialize(val.Get());
}

template <HasUUID T>
void Serializer::Deserialize(RefPtr<T>& val)
{
    Deserialize(val.GetPtrRef());
}

template <HasUUID T>
void Serializer::Deserialize(RefPtr<T>& val, const ReferenceResolveCallback& callback)
{
    Deserialize(val.GetPtrRef(), callback);
}

template <IsSerializable T>
void Serializer::Serialize(T& val)
{
    val.Serialize(this);
}

template <IsSerializable T>
void Serializer::Deserialize(T& val)
{
    val.Deserialize(this);
}
} // namespace Engine
