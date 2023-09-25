#pragma once
#include "Core/Object.hpp"
#include "Libs/Ptr.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include <concepts>
#include <cstddef>
#include <fmt/format.h>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Engine
{

using ReferenceResolveCallback = std::function<void(void* resource)>;

template <class T>
concept HasUUID = requires(T* a, UUID uuid) { a->GetUUID(); };

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
    // used for deserialization
    Serializer(const std::vector<uint8_t>& data, SerializeReferenceResolveMap* resolve) : resolveCallbacks(resolve) {}

    // used for serialization
    Serializer(){};

    virtual ~Serializer() {}

    // template <class T>
    // void Serialize(std::string_view name, const T& val)
    // {
    //     Serialize(name, (unsigned char*)&val, sizeof(T));
    // }
    //
    // template <class T>
    // void Deserialize(std::string_view name, T& val)
    // {
    //     Deserialize(name, (unsigned char*)&val, sizeof(T));
    // }
    //

    template <class T, class U>
    void Serialize(std::string_view, const std::unordered_map<T, U>& val);
    template <class T, class U>
    void Deserialize(
        std::string_view, std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback = nullptr
    );

    template <class T>
    void Serialize(std::string_view name, const std::vector<T>& val);
    template <class T>
    void Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback = nullptr);

    template <class T>
    void Serialize(std::string_view name, const std::unique_ptr<T>& val);
    template <class T>
    void Deserialize(std::string_view name, std::unique_ptr<T>& val);

    template <IsSerializable T>
    void Serialize(std::string_view name, const T& val);
    template <IsSerializable T>
    void Deserialize(std::string_view name, T& val);

    template <HasUUID T>
    void Serialize(std::string_view name, T* val);
    template <HasUUID T>
    void Deserialize(std::string_view name, T*& val);
    template <HasUUID T>
    void Deserialize(std::string_view name, T*& val, const ReferenceResolveCallback& callback);

    template <HasUUID T>
    void Serialize(std::string_view name, RefPtr<T> val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val, const ReferenceResolveCallback& callback);

    virtual void Serialize(std::string_view name, const std::string& val) = 0;
    virtual void Deserialize(std::string_view name, std::string& val) = 0;

    virtual void Serialize(std::string_view name, const UUID& uuid) = 0;
    virtual void Deserialize(std::string_view name, UUID& uuid) = 0;

    virtual void Serialize(std::string_view name, const uint32_t& v) = 0;
    virtual void Deserialize(std::string_view name, uint32_t& v) = 0;

    virtual void Serialize(std::string_view name, const int32_t& v) = 0;
    virtual void Deserialize(std::string_view name, int32_t& v) = 0;

    virtual void Serialize(std::string_view name, const float& v) = 0;
    virtual void Deserialize(std::string_view name, float& v) = 0;

    virtual void Serialize(std::string_view name, const glm::mat4& v) = 0;
    virtual void Deserialize(std::string_view name, glm::mat4& v) = 0;

    virtual void Serialize(std::string_view name, const glm::quat& v) = 0;
    virtual void Deserialize(std::string_view name, glm::quat& v) = 0;

    virtual void Serialize(std::string_view name, const glm::vec4& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec4& v) = 0;

    virtual void Serialize(std::string_view name, const glm::vec3& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec3& v) = 0;

    virtual void Serialize(std::string_view name, nullptr_t) = 0;
    virtual bool IsNull(std::string_view name) = 0;

    virtual std::vector<uint8_t> GetBinary() = 0;
    const std::unordered_map<UUID, Object*>& GetContainedObjects()
    {
        return objects;
    }

protected:
    SerializeReferenceResolveMap* resolveCallbacks;
    std::unordered_map<UUID, Object*> objects;

    virtual void Serialize(std::string_view name, unsigned char* p, size_t size) = 0;
    virtual void Deserialize(std::string_view name, unsigned char* p, size_t size) = 0;

    virtual std::unique_ptr<Serializer> CreateSubserializer() = 0;
    virtual void AppendSubserializer(std::string_view name, Serializer* s) = 0;
    virtual std::unique_ptr<Serializer> CreateSubdeserializer(std::string_view name) = 0;
};

template <class T>
concept HasReferenceResolveParamter =
    requires(std::string_view name, Serializer* s, T v, SerializeReferenceResolve r) { s->Deserialize(name, v, r); };

template <class T, class U>
void Serializer::Serialize(std::string_view name, const std::unordered_map<T, U>& val)
{
    uint32_t size = val.size();
    std::string path = fmt::format("{}/size", name);
    Serialize(path.data(), size);
    int i = 0;
    for (auto& iter : val)
    {
        std::string keypath = fmt::format("{}/{}_key", name, i);
        Serialize(keypath, iter.first);

        std::string valuepath = fmt::format("{}/{}_value", name, i);
        Serialize(valuepath, iter.second);
        i += 1;
    }
}

template <class T, class U>
void Serializer::Deserialize(
    std::string_view name, std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback
)
{
    uint32_t size;
    auto sizepath = fmt::format("{}/size", name);
    Deserialize(sizepath.data(), size);
    for (int i = 0; i < size; ++i)
    {
        T key;
        U value;
        std::string keypath = fmt::format("{}/{}_key", name, i);
        Deserialize(keypath, key);

        std::string valuepath = fmt::format("{}/{}_value", name, i);
        if constexpr (HasReferenceResolveParamter<U>)
            Deserialize(valuepath, val[key], callback);
        else
            Deserialize(valuepath, val[key]);
    }
}

template <class T>
void Serializer::Serialize(std::string_view name, const std::vector<T>& val)
{
    uint32_t size = val.size();
    std::string sizepath = fmt::format("{}/size", name);
    Serialize(sizepath, size);
    for (int i = 0; i < val.size(); ++i)
    {
        std::string s = fmt::format("{}/data/{}", name, i);
        Serialize(s, val[i]);
    }
}

template <class T>
void Serializer::Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback)
{
    uint32_t size;
    std::string sizepath = fmt::format("{}/size", name);
    Deserialize(sizepath, size);
    val.resize(size);
    for (int i = 0; i < size; ++i)
    {
        std::string path = fmt::format("{}/data/{}", name, i);
        if constexpr (HasReferenceResolveParamter<T>)
            Deserialize(path, val[i], callback);
        else
            Deserialize(path, val[i]);
    }
}

template <class T>
void Serializer::Serialize(std::string_view name, const std::unique_ptr<T>& val)
{
    if (val == nullptr)
        Serialize(name, nullptr);

    if constexpr (std::is_abstract_v<T>)
    {
        std::string path = fmt::format("{}/objectTypeID", name);
        Serialize(path, val->GetObjectTypeID());
        path = fmt::format("{}/object", name);
        Serialize(path, *val);
    }
    else
        Serialize(name, *val);
}

template <class T>
void Serializer::Deserialize(std::string_view name, std::unique_ptr<T>& val)
{
    // if it's a null it's should stay as null
    if (!IsNull(name))
    {
        T* newVal = nullptr;
        if constexpr (std::is_abstract_v<T>)
        {
            std::string path = fmt::format("{}/objectTypeID", name);
            ObjectTypeID id;
            Deserialize(path, id);
            auto obj = ObjectRegistry::CreateObject(id);
            if (obj)
            {
                auto objPtr = obj.release();
                val.reset(static_cast<T*>(objPtr));
            }

            path = fmt::format("{}/object", name);
            Deserialize(path, *val);
        }
        else
        {
            newVal = new T();
            val.reset(newVal);
            Deserialize(name, *val);
        }

        if constexpr (std::is_base_of_v<Object, T>)
        {
            objects[val->GetUUID()] = val.get();
        }
    }
}

template <IsSerializable T>
void Serializer::Serialize(std::string_view name, const T& val)
{
    auto s = CreateSubserializer();
    val.Serialize(s.get());
    AppendSubserializer(name, s.get());
}

template <IsSerializable T>
void Serializer::Deserialize(std::string_view name, T& val)
{
    auto s = CreateSubdeserializer(name);
    val.Deserialize(s.get());
    auto subcontained = s->GetContainedObjects();
    for (auto& iter : subcontained)
    {
        objects[iter.first] = iter.second;
    }

    if constexpr (std::is_base_of_v<Object, T>)
    {
        objects[val.GetUUID()] = &val;
    }
}

template <HasUUID T>
void Serializer::Serialize(std::string_view name, T* val)
{
    if (val)
        Serialize(name, val->GetUUID());
    else
        Serialize(name, UUID::GetEmptyUUID());
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, T*& val)
{
    UUID uuid;
    Deserialize(name, uuid);
    val = nullptr;
    if (resolveCallbacks && uuid != UUID::GetEmptyUUID())
    {
        (*resolveCallbacks)[uuid].emplace_back((void*&)val, uuid, nullptr);
    }
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, T*& val, const ReferenceResolveCallback& callback)
{
    UUID uuid;
    Deserialize(name, uuid);
    val = nullptr;
    if (resolveCallbacks && uuid != UUID::GetEmptyUUID())
    {
        (*resolveCallbacks)[uuid].emplace_back((void*&)val, uuid, callback);
    }
}

template <HasUUID T>
void Serializer::Serialize(std::string_view name, RefPtr<T> val)
{
    const T* tval = val.Get();
    Serialize(name, tval);
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
