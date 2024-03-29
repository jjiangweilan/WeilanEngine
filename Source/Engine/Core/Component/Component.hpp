#pragma once
#include "Core/Asset.hpp"
#include "Libs/Ptr.hpp"
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
namespace Engine
{
class GameObject;
class Component : public Object, public Serializable
{
public:
    Component(GameObject* gameObject);
    virtual ~Component() = 0;
    virtual void Tick(){};

    virtual const std::string& GetName() = 0;
    virtual std::unique_ptr<Component> Clone(GameObject& owner) = 0;
    GameObject* GetGameObject();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

protected:
    GameObject* gameObject;

    friend class GameObject;
};
} // namespace Engine
