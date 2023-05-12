#pragma once
#include "Core/Resource.hpp"
#include "Libs/Ptr.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
namespace Engine
{
class GameObject;
class Component : public Object
{
public:
    Component(std::string_view name, RefPtr<GameObject> gameObject);
    virtual ~Component() = 0;
    virtual void Tick(){};
    GameObject* GetGameObject();
    const std::string& GetName() { return name; }

protected:
    GameObject* gameObject;
    std::string name;

    friend class GameObject;
    friend struct SerializableField<Component>;
};

template <>
struct SerializableField<Component>
{
    static void Serialize(Component* v, Serializer* s)
    {
        SerializableField<Object>::Serialize(v, s);
        s->Serialize(v->gameObject);
    }

    static void Deserialize(Component* v, Serializer* s)
    {
        SerializableField<Object>::Deserialize(v, s);
        s->Deserialize(v->gameObject);
    }
};

} // namespace Engine
