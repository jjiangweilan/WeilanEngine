#pragma once
#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include <concepts>
#include <cstddef>
#include <fmt/format.h>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine
{

using ReferenceResolveCallback = std::function<void(void* resource)>;

template <class T>
concept HasUUID = requires(T a, UUID uuid) {
                      a.GetUUID();
                      a.SetUUID(uuid);
                  };

template <class T>
concept IsSerializable = std::derived_from<T, Serializable>;

template <class T>
struct HasUUIDContained : std::false_type
{};

template <template <class...> class T, HasUUID U>
struct HasUUIDContained<T<U>> : std::true_type
{};

struct SerializeReferenceResolve
{
    SerializeReferenceResolve(void*& target, const UUID& targetUUID, const ReferenceResolveCallback& callback)
        : target(target), targetUUID(targetUUID), callback(callback){};
    void*& target;
    UUID targetUUID;
    ReferenceResolveCallback callback;
};

using SerializeReferenceResolveMap = std::unordered_map<UUID, std::vector<SerializeReferenceResolve>>;

class Serializer
{
public:
    Serializer(SerializeReferenceResolveMap* resolve) : resolveCallbacks(resolve) {}
    virtual ~Serializer() {}

    template <class T>
    void Serialize(std::string_view name, const T& val)
    {
        Serialize(name, (unsigned char*)&val, sizeof(T));
    }
    template <class T>
    void Deserialize(std::string_view name, T& val)
    {
        Deserialize(name, (unsigned char*)&val, sizeof(T));
    }

    template <class T, class U>
    void Serialize(std::string_view, const std::unordered_map<T, U>& val);
    template <class T, class U>
    void Deserialize(std::string_view,
                     std::unordered_map<T, U>& val,
                     const ReferenceResolveCallback& callback = nullptr);

    template <class T>
    void Serialize(std::string_view name, const std::vector<T>& val);
    template <class T>
    void Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback = nullptr);

    template <class T>
    void Serialize(std::string_view name, const std::unique_ptr<T>& val);
    template <class T>
    void Deserialize(std::string_view name, std::unique_ptr<T>& val);

    template <IsSerializable T>
    void Serialize(std::string_view name, T& val);
    template <IsSerializable T>
    void Deserialize(std::string_view name, T& val);

    template <HasUUID T>
    void Serialize(std::string_view name, T* val);
    template <HasUUID T>
    void Deserialize(std::string_view name, T*& val, const ReferenceResolveCallback& callback = nullptr);

    template <HasUUID T>
    void Serialize(std::string_view name, RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val, const ReferenceResolveCallback& callback);

    virtual void Serialize(std::string_view name, const std::string& val) = 0;
    virtual void Deserialize(std::string_view name, std::string& val) = 0;

    virtual void Serialize(std::string_view name, const UUID& uuid) = 0;
    virtual void Deserialize(std::string_view name, UUID& uuid) = 0;

    virtual std::vector<unsigned char> GetBinary() = 0;

protected:
    SerializeReferenceResolveMap* resolveCallbacks;

    virtual void Serialize(std::string_view name, unsigned char* p, size_t size) = 0;
    virtual void Deserialize(std::string_view name, unsigned char* p, size_t size) = 0;
};

template <class T>
concept HasReferenceResolveParamter =
    requires(std::string_view name, Serializer* s, T v, SerializeReferenceResolve r) { s->Deserialize(name, v, r); };

template <class T, class U>
void Serializer::Serialize(std::string_view name, const std::unordered_map<T, U>& val)
{
    uint32_t size = val.size();
    auto path = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(path), "{}/size", name);
    Serialize(path.data(), size);
    int i = 0;
    for (auto& iter : val)
    {
        auto keypath = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(keypath), "{}/{}_key", name, i);
        Serialize(keypath.data(), iter.first);

        auto valuepath = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(valuepath), "{}/{}_value", name, i);
        Serialize(valuepath.data(), iter.second);
        i += 1;
    }
}

template <class T, class U>
void Serializer::Deserialize(std::string_view name,
                             std::unordered_map<T, U>& val,
                             const ReferenceResolveCallback& callback)
{
    uint32_t size;
    auto sizepath = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(sizepath), "{}/size", name);
    Deserialize(name, size);
    for (int i = 0; i < size; ++i)
    {
        T key;
        U value;
        auto keypath = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(keypath), "{}/{}_key", name, i);
        Deserialize(keypath.data(), key);

        auto valuepath = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(valuepath), "{}/{}_value", name, i);
        if constexpr (HasReferenceResolveParamter<U>)
            Deserialize(val[key], callback);
        else
            Deserialize(valuepath.data(), val[key]);
    }
}

template <class T>
void Serializer::Serialize(std::string_view name, const std::vector<T>& val)
{
    uint32_t size;
    auto path = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(path), "{}/size", name);
    for (int i = 0; i < val.size(); ++i)
    {
        auto path = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(path), "{}/{}", name, i);
        Serialize(path.data(), val[i]);
    }
}

template <class T>
void Serializer::Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback)
{
    uint32_t size;
    auto path = fmt::memory_buffer();
    fmt::format_to(std::back_inserter(path), "{}/size", name);
    Deserialize(path.data(), size);
    val.resize(size);
    for (int i = 0; i < size; ++i)
    {
        auto path = fmt::memory_buffer();
        fmt::format_to(std::back_inserter(path), "{}/{}", name, i);
        if constexpr (HasReferenceResolveParamter<T>)
            Deserialize(path.data(), val[i], callback);
        else
            Deserialize(path.data(), val[i]);
    }
}

template <class T>
void Serializer::Serialize(std::string_view name, const std::unique_ptr<T>& val)
{
    Serialize(name, *val);
}

template <class T>
void Serializer::Deserialize(std::string_view name, std::unique_ptr<T>& val)
{
    Deserialize(name, *val);
}

template <IsSerializable T>
void Serializer::Serialize(std::string_view name, T& val)
{
    val.Serialize(name, this);
}

template <IsSerializable T>
void Serializer::Deserialize(std::string_view name, T& val)
{
    val.Deserialize(name, this);
}

template <HasUUID T>
void Serializer::Serialize(std::string_view name, T* val)
{
    if (val)
        Serialize(name, val->GetUUID());
    else
        Serialize(name, UUID::empty);
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, T*& val, const ReferenceResolveCallback& callback)
{
    UUID uuid;
    Deserialize(name, uuid);
    val = nullptr;
    if (resolveCallbacks && uuid != UUID::empty)
    {
        (*resolveCallbacks)[uuid].emplace_back((void*&)val, uuid, callback);
    }
}

template <HasUUID T>
void Serializer::Serialize(std::string_view name, RefPtr<T>& val)
{
    Serialize(name, val.Get());
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, RefPtr<T>& val)
{
    Deserialize(name, val.GetPtrRef());
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, RefPtr<T>& val, const ReferenceResolveCallback& callback)
{
    Deserialize(name, val.GetPtrRef(), callback);
}
} // namespace Engine
