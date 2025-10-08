#pragma once
#include "Core/Object.hpp"
#include "Core/Ptr.hpp"
#include "Libs/DynamicArray.hpp"
#include "Libs/UUID.hpp"
#include "Serializable.hpp"
#include "nlohmann/json_fwd.hpp"
#include <concepts>
#include <cstddef>
#include <fmt/format.h>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

using ReferenceResolveCallback = std::function<void(void* resource)>;

template <class T>
concept HasUUID = requires(T* a, UUID uuid) { a->GetUUID(); };

template <class T>
concept IsSerializableClass = std::derived_from<T, Serializable>;

template <class T>
concept HasSerializeFunc = requires(T a, Serializer* s) {
    a.Serialize(s);
    a.Deserialize(s);
};

template <class T>
concept IsSerializable =
    IsSerializableClass<T> ||
    HasSerializeFunc<T>; // use with CanBeSerializerParameter for full test, Serializer itself uses this only

template <class T>
struct HasUUIDContained : std::false_type
{};

template <template <class...> class T, HasUUID U>
struct HasUUIDContained<T<U>> : std::true_type
{};

struct SerializeReferenceResolve
{
    SerializeReferenceResolve(void** target, const UUID& targetUUID, const ReferenceResolveCallback& callback)
        : target(target), targetUUID(targetUUID), callback(callback) {};
    // add holder's UUID here so that we can check if the holder is still alive when we resolve target
    // note: holding a directly pointer doesn't work for moved object even if holder's UUID is checked.
    // maybe consider using pointer to member?
    void** target = nullptr;
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
    Serializer() {};

    virtual ~Serializer() {}

    template <class T, class U>
    void Serialize(std::string_view, const std::unordered_map<T, U>& val);
    template <class T, class U>
    void Deserialize(
        std::string_view, std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback = nullptr
    );

    template <class T>
    void Serialize(std::string_view name, const std::vector<T>& val, std::function<bool(const T&)> = nullptr);
    template <class T>
    void Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback = nullptr);

    template <class T>
    void Serialize(std::string_view name, const ObjPtr<T>& val);
    template <class T>
    void Deserialize(std::string_view name, ObjPtr<T>& val);

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
    void Deserialize(std::string_view name, std::nullptr_t, const ReferenceResolveCallback& callback);

    template <HasUUID T>
    void Serialize(std::string_view name, RefPtr<T> val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val);
    template <HasUUID T>
    void Deserialize(std::string_view name, RefPtr<T>& val, const ReferenceResolveCallback& callback);

    virtual void Serialize(std::string_view name, const std::string& val) = 0;
    virtual void Deserialize(std::string_view name, std::string& val) = 0;

    virtual void Serialize(std::string_view name, const nlohmann::json& json) = 0;
    virtual void Deserialize(std::string_view name, nlohmann::json& json) = 0;

    virtual void Serialize(std::string_view name, const bool val) = 0;
    virtual void Deserialize(std::string_view name, bool& val) = 0;

    virtual void Serialize(std::string_view name, const UUID& uuid) = 0;
    virtual void Deserialize(std::string_view name, UUID& uuid) = 0;

    virtual void Serialize(std::string_view name, const uint32_t& v) = 0;
    virtual void Deserialize(std::string_view name, uint32_t& v) = 0;

    virtual void Serialize(std::string_view name, const int32_t& v) = 0;
    virtual void Deserialize(std::string_view name, int32_t& v) = 0;

    virtual void Serialize(std::string_view name, const uint64_t& v) = 0;
    virtual void Deserialize(std::string_view name, uint64_t& v) = 0;

    virtual void Serialize(std::string_view name, const int64_t& v) = 0;
    virtual void Deserialize(std::string_view name, int64_t& v) = 0;

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

    virtual void Serialize(std::string_view name, const glm::vec2& v) = 0;
    virtual void Deserialize(std::string_view name, glm::vec2& v) = 0;

    virtual void Serialize(std::string_view name, nullptr_t) = 0;
    virtual bool IsNull(std::string_view name) = 0;
    virtual bool IsNull() = 0;

    virtual std::vector<uint8_t> GetBinary() = 0;
    const std::unordered_map<UUID, Object*>& GetContainedObjects() { return objects; }

    const std::vector<UUID>& GetReferencedObjects() { return referencedObjects; }

    virtual std::unique_ptr<Serializer> CreateSubserializer() = 0;
    virtual std::unique_ptr<Serializer> CreateSubdeserializer(std::string_view name) = 0;
    virtual void AppendSubserializer(std::string_view name, Serializer* s) = 0;

protected:
    SerializeReferenceResolveMap* resolveCallbacks;
    std::unordered_map<UUID, Object*> objects;
    std::vector<UUID> referencedObjects;

    virtual void Serialize(std::string_view name, unsigned char* p, size_t size) = 0;
    virtual void Deserialize(std::string_view name, unsigned char* p, size_t size) = 0;

    virtual size_t GetArraySize(std::string_view name) = 0;

    // this is stupid, but I don't know how to work around unordered_map deserialization
    virtual const nlohmann::json& GetJsonObject(std::string_view name) = 0;
};

template <class T>
concept HasReferenceResolveCallbackParamter =
    requires(std::string_view name, Serializer* s, T v, ReferenceResolveCallback r) { s->Deserialize(name, v, r); };

template <class T, class U>
void Serializer::Serialize(std::string_view name, const std::unordered_map<T, U>& val)
{
    for (auto& iter : val)
    {
        Serialize(fmt::format("{}/{}", name, iter.first), iter.second);
    }
}

template <class T, class U>
void Serializer::Deserialize(
    std::string_view name, std::unordered_map<T, U>& val, const ReferenceResolveCallback& callback
)
{
    auto& j = GetJsonObject(name);
    for (auto& item : j.items())
    {
        Deserialize(fmt::format("{}/{}", name, item.key()), val[item.key()]);
    }
}

template <class T>
void Serializer::Serialize(
    std::string_view name, const std::vector<T>& val, std::function<bool(const T&)> shouldSerialize
)
{
    int serializeIndex = 0;
    for (int i = 0; i < val.size(); ++i)
    {
        bool serializeThis = shouldSerialize ? shouldSerialize(val[i]) : true;
        if (serializeThis)
        {
            std::string s = fmt::format("{}/{}", name, serializeIndex);
            Serialize(s, val[i]);
            serializeIndex += 1;
        }
    }
}

template <class T>
void Serializer::Deserialize(std::string_view name, std::vector<T>& val, const ReferenceResolveCallback& callback)
{
    uint32_t size = GetArraySize(name);
    val.resize(size);
    for (int i = 0; i < size; ++i)
    {
        std::string path = fmt::format("{}/{}", name, i);
        if constexpr (HasReferenceResolveCallbackParamter<T>)
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
        bool valid = false;
        if constexpr (std::is_abstract_v<T>)
        {
            std::string path = fmt::format("{}/objectTypeID", name);
            ObjectTypeID id;
            Deserialize(path, id);
            auto obj = ObjectRegistry::CreateObject(id);
            // do type check in dev environment
            Object* casted = obj.get();
#if ENGINE_DEV_BUILD
            casted = dynamic_cast<T*>(obj.get());
#endif
            if (casted)
            {
                auto objPtr = obj.release();
                val.reset(static_cast<T*>(objPtr));

                path = fmt::format("{}/object", name);
                Deserialize(path, *val);
                valid = true;
            }
            else
                val = nullptr;
        }
        else
        {
            newVal = new T();
            val.reset(newVal);
            Deserialize(name, *val);
            valid = true;
        }

        if constexpr (std::is_base_of_v<Object, T>)
        {
            if (valid)
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
    if (!s->IsNull())
    {
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

        referencedObjects
            .insert(referencedObjects.end(), s->GetReferencedObjects().begin(), s->GetReferencedObjects().end());
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
    UUID uuid = UUID::GetEmptyUUID();
    Deserialize(name, uuid);
    val = nullptr;
    if (resolveCallbacks && uuid != UUID::GetEmptyUUID())
    {
        (*resolveCallbacks)[uuid].emplace_back((void**)&val, uuid, nullptr);
    }
}

template <class T>
void Serializer::Serialize(std::string_view name, const ObjPtr<T>& val)
{
    if (val)
        Serialize(name, val->GetUUID());
    else
        Serialize(name, UUID::GetEmptyUUID());
}

template <class T>
void Serializer::Deserialize(std::string_view name, ObjPtr<T>& val)
{
    UUID uuid = UUID::GetEmptyUUID();
    Deserialize(name, uuid);
    val = uuid;

    referencedObjects.push_back(uuid);
}

template <HasUUID T>
void Serializer::Deserialize(std::string_view name, T*& val, const ReferenceResolveCallback& callback)
{
    UUID uuid;
    Deserialize(name, uuid);
    val = nullptr;
    if (resolveCallbacks && uuid != UUID::GetEmptyUUID())
    {
        (*resolveCallbacks)[uuid].emplace_back((void**)&val, uuid, callback);
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

template <class T>
concept CanBeSerializerParameter = requires(T a, Serializer* s) {
    s->Serialize("", a);
    s->Deserialize("", a);
};
