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
class Component : public Object, Serializable
{
public:
    Component(std::string_view name, RefPtr<GameObject> gameObject);
    virtual ~Component() = 0;
    virtual void Tick(){};
    GameObject* GetGameObject();
    const std::string& GetName() { return name; }

    void Serialize(Serializer* s) const override
    {
        s->Serialize("uuid", uuid);
        s->Serialize("gameObject", gameObject);
    }

    void Deserialize(Serializer* s) override
    {
        s->Deserialize("uuid", uuid);
        s->Deserialize("gameObject", gameObject);
    }

protected:
    GameObject* gameObject;
    std::string name;

    friend class GameObject;
};
} // namespace Engine
