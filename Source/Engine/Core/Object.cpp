#include "Object.hpp"
#include "Engine/Library/Serialization/Serializer.hpp"
Object::EngineObjectMap Object::GetAllEngineObjects()
{
    Object::EngineObjectMap map;
    spdlog::info("calling GetAllEngineObjects()");

    for (auto& v : ObjectTracker::Singleton().GetUUIDToSlotIndex())
    {
        auto obj = ObjectTracker::Singleton().GetObject(v.second);
        if (obj != nullptr)
        {
            map[v.first] = obj;
        }
    }

    return map;
}

std::unordered_map<std::string, ObjectTypeInfo*>* ObjectRegistry::GetObjectTypeRegistryByName()
{
    static std::unique_ptr<std::unordered_map<std::string, ObjectTypeInfo*>> registeredObject =
        std::make_unique<std::unordered_map<std::string, ObjectTypeInfo*>>();
    return registeredObject.get();
}

const ObjectTypeInfo* ObjectRegistry::GetObjectTypeInfo(const UUID& typeID)
{
    std::unordered_map<ObjectTypeID, std::unique_ptr<ObjectTypeInfo>>& registry = *GetObjectTypeInfoRegistry();

    auto iter = registry.find(typeID);
    if (iter == registry.end())
        return nullptr;

    return iter->second.get();
}

ObjectTypeInfo* ObjectRegistry::GetObjectTypeInfoPrivate(const UUID& typeID)
{
    std::unordered_map<ObjectTypeID, std::unique_ptr<ObjectTypeInfo>>& registry = *GetObjectTypeInfoRegistry();

    auto iter = registry.find(typeID);
    ObjectTypeInfo* ret = nullptr;
    if (iter == registry.end())
    {
        std::unique_ptr<ObjectTypeInfo> info = std::make_unique<ObjectTypeInfo>();
        ret = info.get();
        registry[typeID] = std::move(info);
    }
    else
    {
        ret = iter->second.get();
    }

    return ret;
}

std::unordered_map<ObjectTypeID, std::unique_ptr<ObjectTypeInfo>>* ObjectRegistry::GetObjectTypeInfoRegistry()
{
    static std::unordered_map<ObjectTypeID, std::unique_ptr<ObjectTypeInfo>> registry;
    return &registry;
}

std::unique_ptr<Object> ObjectRegistry::CreateObject(const ObjectTypeID& id)
{
    auto typeInfo = GetObjectTypeInfo(id);
    if (typeInfo != nullptr)
    {
        return typeInfo->CreateInstance();
    }

    return nullptr;
}

std::unique_ptr<Object> ObjectRegistry::CreateObjectByName(std::string_view name)
{
    auto t = GetObjectTypeRegistryByName();
    auto typeInfo = t->find(std::string(name));
    if (typeInfo != t->end())
    {
        return typeInfo->second->CreateInstance();
    }

    return nullptr;
}

std::vector<std::string>& ObjectRegistry::GetComponentTypeNamesRegistry()
{
    static std::vector<std::string> s{};
    return s;
}

void Object::Serialize(Serializer* s) const
{
    s->Serialize("uuid", uuid);
    s->Serialize("name", name);
}

void Object::Deserialize(Serializer* s)
{
    UUID uuid;
    s->Deserialize("uuid", uuid);
    s->Deserialize("name", name);
    ObjectTracker::Singleton().ReplaceObjectUUID(this, uuid);
}

const char Object::_objectRegister = ObjectRegistry::RegisterBaseObject(
    StaticGetObjectTypeID(),
    "Object",
    []()
    { return nullptr; }
);

const ObjectTypeID& Object::StaticGetObjectTypeID()
{
    static const UUID uuid = UUID("6F574346-3464-4A2F-AFA6-32E0442C4EFB");
    return uuid;
}
const ObjectTypeID& Object::GetObjectTypeID() const
{
    return Object::StaticGetObjectTypeID();
}
const std::string& Object::StaticGetTypeName()
{
    static std::string typeName = "Object";
    return typeName;
}
const std::string& Object::GetTypeName() const
{
    return StaticGetTypeName();
}

const ObjectTypeInfo* Object::GetTypeInfo() const
{
    static const ObjectTypeInfo* info = ObjectRegistry::GetObjectTypeInfo(StaticGetObjectTypeID());
    return info;
}

std::unordered_map<ObjectTypeID, std::unique_ptr<ITypeReflection>>& ObjectRegistry::GetTypeReflectionInstances()
{
    static std::unordered_map<ObjectTypeID, std::unique_ptr<ITypeReflection>> typeReflections;
    return typeReflections;
}

void ObjectTypeInfo::GetVariable(Object& obj, const std::string& name, void*& ptr) const
{
    auto parent = parentTypeInfo;
    auto activeTypeReflection = typeReflection;

    while (activeTypeReflection != nullptr)
    {
        const auto& variables = activeTypeReflection->GetVariables();
        auto iter = variables.find(name);
        if (iter != variables.end())
        {
            const auto& metadata = iter->second;
            metadata.getter((void*)&obj, ptr);
            return;
        }

        if (parent != nullptr)
        {
            activeTypeReflection = parent->GetTypeReflection();
            parent = parent->GetParentTypeInfo();
        }
        else
        {
            activeTypeReflection = nullptr;
        }
    }
}

void Object::SerializeByReflection(Serializer* s)
{
    auto typeInfo = GetTypeInfo();
    typeInfo->Serialize(*this, *s);
}

void Object::DeserializeByReflection(Serializer* s)
{
    // because we register the object before deserializing, we need to
    // temporarily store the old uuid and restore it after deserialization
    auto typeInfo = GetTypeInfo();
    UUID oldUUID = std::move(this->uuid);

    typeInfo->Deserialize(*this, *s);

    UUID newUUILD = std::move(this->uuid);
    this->uuid = std::move(oldUUID);

    ObjectTracker::Singleton().ReplaceObjectUUID(this, newUUILD);
}
